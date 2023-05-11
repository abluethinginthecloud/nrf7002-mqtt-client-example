/*! @file transport.c
 * @brief Implements the MQTT functionalities into the board
 *
 * @author A BLUE THING IN THE CLOUD S.L.U.
 *    ===  When the technology becomes art ===
 *
 * http://abluethinginthecloud.com
 * j.longares@abluethinginthecloud
 *
 * (c) A BLUE THING IN THE CLOUD S.L.U.
 *
 *
 *
        ██████████████    ██    ██    ██  ██████    ██████████████
        ██          ██      ████████████████  ██    ██          ██
        ██  ██████  ██  ██████  ██    ██        ██  ██  ██████  ██
        ██  ██████  ██    ██████    ██      ██      ██  ██████  ██
        ██  ██████  ██      ██      ████  ██████    ██  ██████  ██
        ██          ██    ██      ██████    ████    ██          ██
        ██████████████  ██  ██  ██  ██  ██  ██  ██  ██████████████
                        ██████  ████  ██████  ████
        ██████  ██████████  ████    ████████      ████      ██
        ██  ████  ██    ██  ████        ████    ████████  ██    ██
            ██  ██  ████  ██      ██      ██      ██  ████  ██████
        ████  ████    ██      ██          ████  ██  ██        ██
            ██████████          ██      ██    ██  ████    ██  ████
          ██  ████    ██      ██████    ██  ██████████    ██    ██
        ██  ████  ████████████████  ██    ██        ████████  ████
                ████        ██  ██████  ██████████      ████  ██
        ██████  ████████████████    ████  ██    ██████    ██  ████
            ████████  ██████  ██    ██████      ██        ████  ██
        ██    ██  ████████  ██    ██        ██    ██          ████
          ████  ████          ██      ████████████  ██  ████  ██
        ██  ██████  ████  ██    ██      ████    ██████████
                        ██    ██████    ██      ██      ██  ██████
        ██████████████  ██  ██████  ██  ████  ████  ██  ████  ████
        ██          ██  ██      ████████  ██    ██      ████  ████
        ██  ██████  ██  ████  ██    ██████      ██████████    ████
        ██  ██████  ██    ██████    ██  ██  ████      ████  ██████
        ██  ██████  ██  ████      ██    ████  ██        ████    ██
        ██          ██  ██    ██      ██████████████  ██      ██
        ██████████████  ██████  ██        ██  ████    ██████  ████



*/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/smf.h>
#include "MQTT_helper_custom.h"

#include "message_channel.h"

//! Register log module 
LOG_MODULE_REGISTER( transport, CONFIG_MQTT_SAMPLE_TRANSPORT_LOG_LEVEL );

//! Register subscriber 
ZBUS_SUBSCRIBER_DEFINE(														\
					transport, 												\
					CONFIG_MQTT_SAMPLE_TRANSPORT_MESSAGE_QUEUE_SIZE );

/*! ID for subscribe topic - Used to verify that a subscription succeeded in 
	On_MQTT_Suback().*/ 
#define SUBSCRIBE_TOPIC_ID 2469

//! Priority of the Transport Task thread
#define MQTT_PRIORITY 3
//! Forward declarations 
static const struct smf_state sState[];
//! Connection workqueue 
static void Connect_Work( struct k_work *work );

/*! Define connection work - Used to handle reconnection attempts to the MQTT 
	broker */
static K_WORK_DELAYABLE_DEFINE( connectWork, Connect_Work );

//! Define stack_area of application workqueue 
K_THREAD_STACK_DEFINE( 														\
					stack_area, 											\
					CONFIG_MQTT_SAMPLE_TRANSPORT_WORKQUEUE_STACK_SIZE );

/*! Declare application workqueue. This workqueue is used to call 
 *	MQTT_Helper_Custom_Connect(), and schedule reconnectionn attempts upon 
 *	network loss or disconnection from MQTT.
 */
static struct k_work_q transportQueue;

//! Internal states
enum module_state { MQTT_CONNECTED, MQTT_DISCONNECTED };

//! MQTT client ID buffer 
static const char clientID[ CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID_BUFFER_SIZE ]   
							= CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID;


/*! @struct sObject
 *  @brief User defined state object used to transfer data between state changes
 *  @typedef tObject
 *  @brief Data type with the user state information
*/
static struct tObject {
	// This must be first */
	struct smf_ctx context;
	// Last channel type that a message was received on */
	const struct zbus_channel *channel;
	// Network status */
	enum network_status status;
	// Payload */
	struct payload payload;
} sObject;

//! Used to save the connection state 
static uint8_t stateConnected = MQTT_STATE_UNINIT; 

/*! On_MQTT_Connack is the callback handler from MQTT helper library
 *	activated when there is a new MQTT connection.
 * @brief The functions are called whenever specific MQTT packets are 
 * 		received from the broker, or some library state has changed.
 * 
 * @param[in] enum mqtt_conn_return_code returnCode state of the MQTT
 * 		connection
 */
static void On_MQTT_Connack( enum mqtt_conn_return_code returnCode )
{
	ARG_UNUSED( returnCode );

	smf_set_state( SMF_CTX( &sObject ), &sState[ MQTT_CONNECTED ]);
}

/*! On_MQTT_Disconnect is the callback handler from MQTT helper library
 *	activated when the MQTT is disconnected.
 * @brief The functions are called whenever specific MQTT packets are 
 * 		received from the broker, or some library state has changed.
 * 
 * @param[in] int result (unused argument)
 */
static void On_MQTT_Disconnect( int result )
{
	ARG_UNUSED( result );

	smf_set_state( SMF_CTX( &sObject ), &sState[ MQTT_DISCONNECTED ]);
}

/*! On_MQTT_Publish is the callback handler from MQTT helper library
 *	activated when a message on a subscribed topic is received.
 * @brief The functions are called whenever specific MQTT packets are 
 * 		received from the broker, or some library state has changed.
 * 
 * @param[in] struct mqtt_helper_custom_buf topic received topic
 * 
 * @param[in] struct mqtt_helper_custom_buf payload received payload
 */
static void On_MQTT_Publish(												\
						struct mqtt_helper_custom_buf topic, 				\
						struct mqtt_helper_custom_buf payload )
{
	LOG_INF( "Received payload: %.*s on topic: %.*s", payload.size,
							 payload.ptr,
							 topic.size,
							 topic.ptr );
}

/*! On_MQTT_Suback is the callback handler from MQTT helper library
 *	activated when a new suscription is made.
 * @brief The functions are called whenever specific MQTT packets are 
 * 		received from the broker, or some library state has changed.
 * 
 * @param[in] uint16_t messageID ID from the message
 * 
 * @param[in] int result suscription process result
 */
static void On_MQTT_Suback( uint16_t messageID, int result )
{
	if (( messageID == SUBSCRIBE_TOPIC_ID ) && ( result == 0 )) {
		LOG_INF( "Subscribed to topic %s", 									\
				CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC );
	} else if ( result ) {
		LOG_ERR( "Topic subscription failed, error: %d", result );
	} else {
		LOG_WRN( "Subscribed to unknown topic, id: %d", messageID );
	}
}

/*! Publish publishes MQTT messages on the publishing topic
 * @brief Publish is the function in charge of publishing the messages on 
 *		the	desired topic, specified on the configuration file prj.conf.
 */
static void Publish( void )
{
	int err;
	struct mqtt_publish_param param = {
		.message.payload.data = CONFIG_MQTT_SAMPLE_TRANSPORT_MESSAGE,
		.message.payload.len = strlen( param.message.payload.data ),
		.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		.message_id = CONFIG_MQTT_SAMPLE_TRANSPORT_CLIENT_ID,
		.message.topic.topic.utf8 = CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC,
		.message.topic.topic.size = strlen(									\
								CONFIG_MQTT_SAMPLE_TRANSPORT_PUBLISH_TOPIC ),
	};

	err = MQTT_Helper_custom_Publish( &param );
	if ( err ) {
		LOG_WRN( "Failed to send payload, err: %d", err );
		return;
	}

	LOG_INF( "Published message: \"%.*s\" on topic: \"%.*s\"", 				\
								param.message.payload.len,					\
								param.message.payload.data,					\
								param.message.topic.topic.size,				\
								param.message.topic.topic.utf8);
}

/*! Subscribe makes a new MQTT subscription on the desired topic
 * @brief Subscribe is the function in charge of subscribing to 
 *		the	desired topic, specified on the configuration file prj.conf.
 */
static void Subscribe( void )
{
	int err;
	struct mqtt_topic topics[] = {
		{
			.topic.utf8 = CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC,
			.topic.size = strlen( 											\
							CONFIG_MQTT_SAMPLE_TRANSPORT_SUBSCRIBE_TOPIC ),
		},
	};
	struct mqtt_subscription_list list = {
		.list = topics,
		.list_count = ARRAY_SIZE( topics ),
		.message_id = SUBSCRIBE_TOPIC_ID,
	};

	err = MQTT_Helper_Custom_Subscribe( &list );
	if ( err ) {
		LOG_ERR( "Failed to subscribe to topics, error: %d", err );
		return;
	}
}

/*! Connect_Work estabilishes a connection to the MQTT broker.
* @brief Connect_Work establishes a connection to the MQTT broker and 
*			schedules reconnection attempts.
*
* @param[in] struct k_work *work workqueue thread to schedule the
*			reconnection attempts.
*/
static void Connect_Work( struct k_work *work )
{
	ARG_UNUSED( work );

	int err;
	struct mqtt_helper_custom_conn_params conn_params = {
		.hostname.ptr = CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME,
		.hostname.size = strlen(											\
							CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME ),
		.device_id.ptr = clientID,
		.device_id.size = strlen( clientID ),
	};

	err = MQTT_Helper_Custom_Connect( &conn_params );
	if ( err ) {
		LOG_ERR( "Failed connecting to MQTT, error code: %d", err );
	}

	k_work_reschedule_for_queue( 
				&transportQueue, 											\
				&connectWork,												\
			  	K_SECONDS(													\
					CONFIG_MQTT_SAMPLE_TRANSPORT_RECONNECTION_TIMEOUT_SECONDS));

	stateConnected = MQTT_STATE_CONNECTED; 
}


/*! Disconnected_Entry is the response to the disconnected state entry
* @brief Function executed when the module enters the disconnected state of
*		the Zephyr State Maching framework.
*
* @param[in] void *o unused parameter
*/
static void Disconnected_Entry( void *o )
{
	struct tObject *userObject = o;

	/* Reschedule a connection attempt if we are connected to network and 
	 * we enter the disconnected state.
	 */
	if (userObject->status == NETWORK_CONNECTED) {
		k_work_reschedule_for_queue(										\
									&transportQueue, 						\
									&connectWork, 							\
									K_NO_WAIT);
	}
}


/*! Disconnected_Run is executed during the disconnected state
* @brief Function executed during the disconnected state of the Zephyr 
*		State Maching framework.
*
* @param[in] void *o unused parameter
*/
static void Disconnected_Run( void *o )
{
	struct tObject *userObject = o;

	if (( userObject->status == NETWORK_DISCONNECTED ) && 					\
		( userObject->channel == &NETWORK_CHAN )) {
		/* If NETWORK_DISCONNECTED is received after the MQTT connection is 
		 * closed, we cancel the connect work if it is onging.
		 */
		k_work_cancel_delayable( &connectWork );
	}

	if (( userObject->status == NETWORK_CONNECTED) && 						\
		( userObject->channel == &NETWORK_CHAN )) {

		/* Wait for 5 seconds to ensure that the network stack is ready 
		 * before attempting to connect to MQTT. This delay is only needed 
		 * when building for Wi-Fi.
		 */
		k_work_reschedule_for_queue(										\
									&transportQueue, 						\
									&connectWork, 							\
									K_SECONDS(5));
	}
}

/*! Connected_Entry is the response to the connected state entry
* @brief Function executed when the module enters the connected state of
*		the Zephyr State Maching framework.
*/
static void Connected_Entry( void )
{
	LOG_INF( "Connected to MQTT broker" );
	LOG_INF( "Hostname: %s", CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_HOSTNAME );
	LOG_INF( "Client ID: %s", clientID );
	LOG_INF( "Port: %d", CONFIG_MQTT_HELPER_PORT );
	LOG_INF( "TLS: %s", IS_ENABLED(CONFIG_MQTT_LIB_TLS ) ? "Yes" : "No" );
	LOG_INF( "Username: %s", CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_USERNAME );
	LOG_INF( "Password: %s", CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_PASSWORD );

	/* Cancel any ongoing connect work when we enter connected state */
	k_work_cancel_delayable( &connectWork );

	Subscribe( );
}

/*! Connected_Run is executed during the connected state
* @brief Function executed during the connected state of the Zephyr 
*		State Maching framework.
*
* @param[in] void *o unused parameter
*/
static void Connected_Run( void *o )
{
	struct tObject *userObject = o;

	if (( userObject->status == NETWORK_DISCONNECTED ) && 					\
		( userObject->channel == &NETWORK_CHAN )) {
		/* Explicitly disconnect the MQTT transport when losing network 
		 * connectivity.
		 * This is to cleanup any internal library state.
		 * The call to this function will cause On_MQTT_Disconnect() to be 
		 * called.
		 */
		( void )MQTT_Helper_Custom_Disconnect( );
		return;
	}

	if ( userObject->channel != &PAYLOAD_CHAN ) {
		return;
	}
	Publish( );
}

/*! Connected_Exit is executed when exiting the connected state
* @brief Function executed during the exit of the connected state of 
*		the Zephyr State Maching framework.
*
* @param[in] void *o unused parameter
*/
static void Connected_Exit(void *o)
{
	ARG_UNUSED(o);

	LOG_INF("Disconnected from MQTT broker");
}


/*! @struct sState
 *  @brief State table of the State Machine
*/
static const struct smf_state sState[] = {
	[MQTT_DISCONNECTED] = SMF_CREATE_STATE(									\
										Disconnected_Entry, 				\
										Disconnected_Run, 					\
										NULL),
	[MQTT_CONNECTED] = SMF_CREATE_STATE(									\
										Connected_Entry, 					\
										Connected_Run, 						\
										Connected_Exit),
};

/*! Transport_Task implements the MQTT transport task.
* 
* @brief Transport_Task makes the complete MQTT connection process, 
*		including the MQTT initialization, connection, subscription
*		and publication.
*/
static void Transport_Task( void )
{
	int err;
	const struct zbus_channel *channel;
	enum network_status status;
	struct payload payload;
	struct mqtt_helper_custom_cfg config = {
		.cb = {
			.on_connack 			= 			On_MQTT_Connack,
			.on_disconnect 			= 			On_MQTT_Disconnect,
			.on_publish 			= 			On_MQTT_Publish,
			.on_suback 				= 			On_MQTT_Suback,
		},
	};

	k_work_queue_init( &transportQueue );
	k_work_queue_start(														\
					&transportQueue, 										\
					stack_area,												\
			   		K_THREAD_STACK_SIZEOF( stack_area ),					\
			   		K_HIGHEST_APPLICATION_THREAD_PRIO,						\
			   		NULL);

	err = MQTT_Helper_Custom_Initializer( &config );
	if ( err ) {
		LOG_ERR( "MQTT_Helper_Custom_Initializer, error: %d", err );
		SEND_FATAL_ERROR();
		return;
	}

	/* Set initial state */
	smf_set_initial( SMF_CTX( &sObject ), &sState[ MQTT_DISCONNECTED ]);
	void *k;

	while ( !zbus_sub_wait( &transport, &channel, K_FOREVER )) {
		
		sObject.channel = channel;

		if ( &NETWORK_CHAN == channel ) {

			err = zbus_chan_read( &NETWORK_CHAN, &status, K_SECONDS( 1 ));
			if ( err ) {
				LOG_ERR( "zbus_chan_read, error: %d", err );
				SEND_FATAL_ERROR();
				return;
			}

			sObject.status = status;

			err = smf_run_state( SMF_CTX(&sObject ));
			if ( err ) {
				LOG_ERR( "smf_run_state, error: %d", err );
				SEND_FATAL_ERROR();
				return;
			}
		}

		if ( &PAYLOAD_CHAN == channel ) {

			err = zbus_chan_read( &PAYLOAD_CHAN, &payload, K_SECONDS( 1 ));
			if ( err ) {
				LOG_ERR( "zbus_chan_read, error: %d", err );
				SEND_FATAL_ERROR();
				return;
			}

			sObject.payload = payload;

			err = smf_run_state( SMF_CTX( &sObject ));
			if ( err ) {
				LOG_ERR( "smf_run_state, error: %d", err );
				SEND_FATAL_ERROR();
				return;
			}
			
		}
		
	}
}

K_THREAD_DEFINE(															\
				transport_task_id,											\
				CONFIG_MQTT_SAMPLE_TRANSPORT_THREAD_STACK_SIZE,				\
				Transport_Task, 											\
				NULL, 														\
				NULL, 														\
				NULL, 														\
				MQTT_PRIORITY,												\
				0, 															\
				0);
