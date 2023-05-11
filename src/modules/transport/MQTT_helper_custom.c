/*! @file MQTT_helper_costum.c
 * @brief Custom version of the mqtt_helper library, including username
		and password support on the broker's connection.
 *
 * @author A BLUE THING IN THE CLOUD S.L.U.
 *    ===  When the technology becomes art ===
 *
 * http://abluethinginthecloud.com
 * j.longares@abluethinginthecloud
 *
 * ( c ) A BLUE THING IN THE CLOUD S.L.U.
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
#include <stdlib.h>
#include <string.h>
#include <zephyr/net/socket.h>

#include "MQTT_helper_custom.h"
#include <zephyr/net/mqtt.h>

#if defined( CONFIG_MQTT_HELPER_PROVISION_CERTIFICATES )
#include CONFIG_MQTT_HELPER_CERTIFICATES_FILE
#endif /* CONFIG_MQTT_HELPER_PROVISION_CERTIFICATES */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER( mqtt_helper_custom, CONFIG_MQTT_HELPER_LOG_LEVEL );


/*! Define a custom MQTT_HELPER_STATIC macro that exposes internal variables 
*  when unit testing. */
#define MQTT_HELPER_CUSTOM_STATIC static

/*! Structure with the MQTT Client information */
MQTT_HELPER_CUSTOM_STATIC struct mqtt_client mqttClient;
/*! Broker's socket address */
static struct sockaddr_storage broker;
/*! Receiver buffer */
static char rxBuffer[CONFIG_MQTT_HELPER_RX_TX_BUFFER_SIZE];
/*! Transmitting buffer */
static char txBuffer[CONFIG_MQTT_HELPER_RX_TX_BUFFER_SIZE];
/*! Payload buffer */
MQTT_HELPER_CUSTOM_STATIC char payloadBuffer[
									CONFIG_MQTT_HELPER_PAYLOAD_BUFFER_LEN];
/*! Connection poll semaphor e*/
MQTT_HELPER_CUSTOM_STATIC K_SEM_DEFINE( Connection_Poll_Semaphore, 0, 1 );
/*! MQTT_Helper_Custom configuration */
static struct mqtt_helper_custom_cfg currentConfig;
/*! State machine with the MQTT connection states */
MQTT_HELPER_CUSTOM_STATIC enum mqtt_state mqttState = MQTT_STATE_UNINIT;


/*! State_Name_Get gets the name of the MQTT state.
 * @brief State_Name_Get translates the state of the MQTT connection to
 *		a char buffer
 * 
 * @param[in] enum mqtt_state state, state of the MQTT connection
 * 
 * @return const char * the name of the MQTT state.
 */
static const char *State_Name_Get( enum mqtt_state state )
{
	switch ( state ) {
	case 																	\
	MQTT_STATE_UNINIT: 				return 	"MQTT_STATE_UNINIT";
	case 																	\
	MQTT_STATE_DISCONNECTED: 		return 	"MQTT_STATE_DISCONNECTED";
	case 																	\
	MQTT_STATE_TRANSPORT_CONNECTING:return	"MQTT_STATE_TRANSPORT_CONNECTING";
	case 																	\
	MQTT_STATE_CONNECTING: 			return 	"MQTT_STATE_CONNECTING";
	case 																	\
	MQTT_STATE_TRANSPORT_CONNECTED: return 	"MQTT_STATE_TRANSPORT_CONNECTED";
	case 																	\
	MQTT_STATE_CONNECTED: 			return 	"MQTT_STATE_CONNECTED";
	case 																	\
	MQTT_STATE_DISCONNECTING: 		return 	"MQTT_STATE_DISCONNECTING";
	default: 						return 	"MQTT_STATE_UNKNOWN";
	}
}



/*! MQTT_State_Get is the get method to the MQTT status
 *
 * @return enum mqtt_state MQTT state
 */
MQTT_HELPER_CUSTOM_STATIC enum mqtt_state MQTT_State_Get( void )
{
	return mqttState;
}

/*! MQTT_State_Set is the set method to the MQTT status
 *
 * @param[in] enum mqtt_state newState MQTT state
 */
MQTT_HELPER_CUSTOM_STATIC void MQTT_State_Set( enum mqtt_state newState )
{
	bool notifyError = false;

	if ( MQTT_State_Get( ) == newState ) {
		LOG_DBG( "Skipping transition to the same state ( %s )",			\
			State_Name_Get( MQTT_State_Get( ) ) );
		return;
	}

	/* Check for legal state transitions. */
	switch ( MQTT_State_Get( ) ) {
	case MQTT_STATE_UNINIT:
		if ( newState != MQTT_STATE_DISCONNECTED ) {
			notifyError = true;
		}
		break;
	case MQTT_STATE_DISCONNECTED:
		if ( 																\
			newState != MQTT_STATE_CONNECTING &&							\
		    newState != MQTT_STATE_UNINIT &&								\
		    newState != MQTT_STATE_TRANSPORT_CONNECTING ) {
			notifyError = true;
		}
		break;
	case MQTT_STATE_TRANSPORT_CONNECTING:
		if ( 																\
			newState != MQTT_STATE_TRANSPORT_CONNECTED &&					\
		    newState != MQTT_STATE_DISCONNECTED ) {
			notifyError = true;
		}
		break;
	case MQTT_STATE_CONNECTING:
		if ( 																\
			newState != MQTT_STATE_CONNECTED &&								\
		    newState != MQTT_STATE_DISCONNECTED ) {
			notifyError = true;
		}
		break;
	case MQTT_STATE_TRANSPORT_CONNECTED:
		if ( 																\
			newState != MQTT_STATE_CONNECTING &&							\
		    newState != MQTT_STATE_DISCONNECTED ) {
			notifyError = true;
		}
		break;
	case MQTT_STATE_CONNECTED:
		if ( 																\
			newState != MQTT_STATE_DISCONNECTING &&							\
		    newState != MQTT_STATE_DISCONNECTED ) {
			notifyError = true;
		}
		break;
	case MQTT_STATE_DISCONNECTING:
		if ( newState != MQTT_STATE_DISCONNECTED ) {
			notifyError = true;
		}
		break;
	default:
		LOG_ERR( "New state is unknown: %d", newState );
		notifyError = true;
		break;
	}

	if ( notifyError ) {
		LOG_ERR( "Invalid state transition, %s --> %s",						\
			State_Name_Get( mqttState ),
			State_Name_Get( newState ) );

		__ASSERT( false, "Illegal state transition: %d --> %d", 			\
				mqttState, newState );
	}

	LOG_DBG( "State transition: %s --> %s",									\
		State_Name_Get( mqttState ),										\
		State_Name_Get( newState ) );

	mqttState = newState;
}

/*! MQTT_State_Verify is the verifier method to the MQTT status
 *
 * @param[in] enum mqtt_state state: MQTT state
 * 
 * @return bool
 */
static bool MQTT_State_Verify( enum mqtt_state state )
{
	return ( MQTT_State_Get(  ) == state );
}

/*! Publish_Get_Payload get method to the publish payload
 * @brief mqtt_readall_publish_payload wrapper
 * @param[in] struct mqtt_client *const mqttClient: MQTT client
 * @param[in] size_t length payload's length
 * 
 * @return int payload
 */
static int Publish_Get_Payload( 											\
							struct mqtt_client *const mqttClient, 			\
							size_t length )
{
	if ( length > sizeof( payloadBuffer ) ) {
		LOG_ERR( "Incoming MQTT message too large for payload buffer" );
		return -EMSGSIZE;
	}

	return mqtt_readall_publish_payload( mqttClient, payloadBuffer, length );
}

/*! Send_ACK sends an ACK when a message is received
 * @brief mqtt_publish_qos1_ack wrapper
 * @param[in] struct mqtt_client *const mqttClient: MQTT client
 * @param[in] uint16_t Message_ID: Message ID
 */
static void Send_ACK( struct mqtt_client *const mqttClient, uint16_t Message_ID )
{
	int err;
	const struct mqtt_puback_param ack = {
		.message_id = Message_ID
	};

	err = mqtt_publish_qos1_ack( mqttClient, &ack );
	if ( err ) {
		LOG_WRN( "Failed to send MQTT ACK, error: %d", err );
		return;
	}

	LOG_DBG( "PUBACK sent for message ID %d", Message_ID );
}

/*! On_MQTT_Publish is the callback handler activated when a message on a 
 * subscribed topic is received.
 * 
 * @param[in] struct mqtt_helper_custom_buf topic received topic
 * 
 * @param[in] struct mqtt_helper_custom_buf payload received payload
 */
MQTT_HELPER_CUSTOM_STATIC void On_Publish( const struct mqtt_evt *mqttEvent )
{
	int err;
	const struct mqtt_publish_param *parameters = &mqttEvent->param.publish;
	struct mqtt_helper_custom_buf topic = {
		.ptr 		= 		( char * )parameters->message.topic.topic.utf8,
		.size 		= 		parameters->message.topic.topic.size,
	};
	struct mqtt_helper_custom_buf payload = {
		.ptr = payloadBuffer,
	};
	
	err = Publish_Get_Payload( &mqttClient, parameters->message.payload.len );
	if ( err ) {
		LOG_ERR( "Publish_Get_Payload, error: %d", err );

		if ( currentConfig.cb.on_error ) {
			currentConfig.cb.on_error( MQTT_HELPER_ERROR_MSG_SIZE );
		}

		return;
	}

	if ( parameters->message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE ) {
		Send_ACK( &mqttClient, parameters->message_id );
	}

	payload.size = parameters->message.payload.len;

	if ( currentConfig.cb.on_publish ) {
		currentConfig.cb.on_publish( topic, payload );
	}
}

/*! MQTT_Event_Handler is the callback handler activated with the MQTT events
 * 
 * @param[in] struct mqtt_client *const mqttClient: MQTT Client
 * @param[in] const struct mqtt_evt *mqttEvent: MQTT Events
 */
MQTT_HELPER_CUSTOM_STATIC void MQTT_Event_Handler( 							\
									struct mqtt_client *const mqttClient,	\
			     					const struct mqtt_evt *mqttEvent )
{
	switch ( mqttEvent->type ) {
	case MQTT_EVT_CONNACK:
		LOG_DBG( "MQTT mqttClient connected" );

		if ( mqttEvent->param.connack.return_code == MQTT_CONNECTION_ACCEPTED ) {
			MQTT_State_Set( MQTT_STATE_CONNECTED );
		} else {
			MQTT_State_Set( MQTT_STATE_DISCONNECTED );
		}

		if ( currentConfig.cb.on_connack ) {
			currentConfig.cb.on_connack( mqttEvent->param.connack.return_code );
		}
		break;
	case MQTT_EVT_DISCONNECT:
		LOG_DBG( "MQTT_EVT_DISCONNECT: result = %d", mqttEvent->result );

		MQTT_State_Set( MQTT_STATE_DISCONNECTED );

		if ( currentConfig.cb.on_disconnect ) {
			currentConfig.cb.on_disconnect( mqttEvent->result );
		}
		break;
	case MQTT_EVT_PUBLISH:
		LOG_DBG( "MQTT_EVT_PUBLISH, message ID: %d, len = %d",				\
			mqttEvent->param.publish.message_id,							\
			mqttEvent->param.publish.message.payload.len );
		On_Publish( mqttEvent );
		break;
	case MQTT_EVT_PUBACK:
		LOG_DBG( "MQTT_EVT_PUBACK: id = %d result = %d",					\
			mqttEvent->param.puback.message_id,								\
			mqttEvent->result );

		if ( currentConfig.cb.on_puback ) {
			currentConfig.cb.on_puback( 									\
									mqttEvent->param.puback.message_id,		\
						 			mqttEvent->result );
		}
		break;
	case MQTT_EVT_SUBACK:
		LOG_DBG( "MQTT_EVT_SUBACK: id = %d result = %d",					\
			mqttEvent->param.suback.message_id,								\
			mqttEvent->result );

		if ( currentConfig.cb.on_suback ) {
			currentConfig.cb.on_suback( 									\
									mqttEvent->param.suback.message_id,		\
						 			mqttEvent->result );
		}
		break;
	case MQTT_EVT_PINGRESP:
		LOG_DBG( "MQTT_EVT_PINGRESP" );

		if ( currentConfig.cb.on_pingresp ) {
			currentConfig.cb.on_pingresp( );
		}
		break;
	default:
		break;
	}
}

/*! Broker_Initializer initializes the broker with its parameters
 * 
 * @param[in] struct sockaddr_storage *broker: Socket to the broker's address
 * @param[in] struct mqtt_helper_custom_conn_params *connectionParameters: 
 * 			MQTT's connection parameters
 * 
 * @return int error code
 */
static int Broker_Initializer( 												\
				struct sockaddr_storage *broker,							\
		       	struct mqtt_helper_custom_conn_params *connectionParametes )
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family 				= 				AF_INET,
		.ai_socktype 			= 				SOCK_STREAM
	};

	if ( sizeof( CONFIG_MQTT_HELPER_STATIC_IP_ADDRESS ) > 1 ) {
		connectionParametes->hostname.ptr = CONFIG_MQTT_HELPER_STATIC_IP_ADDRESS;

		LOG_DBG( "Using static IP address: %s", 							\
				CONFIG_MQTT_HELPER_STATIC_IP_ADDRESS );
	} else {
		LOG_DBG( "Resolving IP address for %s", 							\
				connectionParametes->hostname.ptr );
	}

	err = getaddrinfo( 														\
					connectionParametes->hostname.ptr, 						\
					NULL, 													\
					&hints, 												\
					&result );
	if ( err ) {
		LOG_ERR( "getaddrinfo(  ) failed, error %d", err );
		return -err;
	}

	addr = result;

	while ( addr != NULL ) {
		if ( addr->ai_addrlen == sizeof( struct sockaddr_in ) ) {
			struct sockaddr_in *broker4 = ( (struct sockaddr_in *)broker );
			char ipv4_addr[INET_ADDRSTRLEN];

			broker4->sin_addr.s_addr =										\
				( ( struct sockaddr_in * )addr->ai_addr )->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons( CONFIG_MQTT_HELPER_PORT );

			inet_ntop( 														\
					AF_INET, 												\
					&broker4->sin_addr.s_addr, 								\
					ipv4_addr,												\
				  	sizeof( ipv4_addr ) );
			LOG_DBG( "IPv4 Address found %s", ipv4_addr );
			break;
		}

		LOG_DBG( "ai_addrlen is %u, while it should be %u",					\
			( unsigned int )addr->ai_addrlen,								\
			( unsigned int )sizeof( struct sockaddr_in ) );

		addr = addr->ai_next;
		break;
	}

	freeaddrinfo( result );
	LOG_INF( "Broker initialized successfully" );

	return err;
}

/*! Client_Connect launches the MQTT client connection
 * 
 * @param[in] struct mqtt_helper_custom_conn_params *connectionParameters: 
 * 			MQTT's connection parameters
 * 
 * @return int error code
 */
static int Client_Connect( 													\
				struct mqtt_helper_custom_conn_params *connectionParametes )
{
	int err;
	struct mqtt_utf8 userName = {
		.utf8   = 	CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_USERNAME,
		.size 	= 	sizeof( CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_USERNAME ),
	};

	struct mqtt_utf8 password = {
		.utf8 	= 	CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_PASSWORD,
		.size 	= 	sizeof(  CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_PASSWORD ),
	};

	mqtt_client_init( &mqttClient );

	err = Broker_Initializer( &broker, connectionParametes );
	if ( err ) {
		return err;
	}

	if( CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_USERNAME != "noUser" )
	{
		mqttClient.user_name = &userName;
	}
	else
	{
		mqttClient.user_name = NULL; 
	}

	if (  CONFIG_MQTT_SAMPLE_TRANSPORT_BROKER_PASSWORD != "noPassword" ) 
	{
		mqttClient.password = &password; 
	}
	else
	{
		mqttClient.password = NULL; 
	}

	mqttClient.broker	        	= 	&broker;
	mqttClient.evt_cb	        	= 	MQTT_Event_Handler;
	mqttClient.client_id.utf8      	= 	connectionParametes->device_id.ptr;
	mqttClient.client_id.size      	= 	connectionParametes->device_id.size;
	mqttClient.protocol_version    	= 	MQTT_VERSION_3_1_1;
	mqttClient.rx_buf	        	= 	rxBuffer;
	mqttClient.rx_buf_size	        = 	sizeof( rxBuffer );
	mqttClient.tx_buf	        	= 	txBuffer;
	mqttClient.tx_buf_size	        = 	sizeof( txBuffer );
	mqttClient.transport.type		= 	MQTT_TRANSPORT_NON_SECURE;

	MQTT_State_Set( MQTT_STATE_TRANSPORT_CONNECTING );

	err = mqtt_connect( &mqttClient );
	if ( err ) {
		LOG_ERR( "mqtt_connect, error: %d", err );
		return err;
	}

	MQTT_State_Set( MQTT_STATE_TRANSPORT_CONNECTED );

	MQTT_State_Set( MQTT_STATE_CONNECTING );

	if ( IS_ENABLED( CONFIG_MQTT_HELPER_SEND_TIMEOUT ) ) {
		struct timeval timeout = {
			.tv_sec = CONFIG_MQTT_HELPER_SEND_TIMEOUT_SEC
		};

		int socket = mqttClient.transport.tcp.sock;

		err = setsockopt(													\
						socket, 											\
						SOL_SOCKET, 										\
						SO_SNDTIMEO, 										\
						&timeout, 											\
						sizeof( timeout ) );

		if ( err == -1 ) {
			LOG_WRN( "Failed to set timeout, errno: %d", errno );

			/* Don't propagate this as an error. */
			err = 0;
		} else {
			LOG_DBG( "Using send socket timeout of %d seconds",
				CONFIG_MQTT_HELPER_SEND_TIMEOUT_SEC );
		}
	}

	return 0;
}

/* Public API */

/*! @brief Initialize the MQTT helper.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Initializer( 										\
								struct mqtt_helper_custom_cfg *configuration )
{
	__ASSERT_NO_MSG( configuration != NULL );

	if ( !MQTT_State_Verify( MQTT_STATE_UNINIT ) && 						\
		!MQTT_State_Verify( MQTT_STATE_DISCONNECTED ) ) {
		LOG_ERR( "Library is in the wrong state ( %s ), %s required",		\
			State_Name_Get( MQTT_State_Get(  ) ),							\
			State_Name_Get( MQTT_STATE_UNINIT ) );

		return -EOPNOTSUPP;
	}

	currentConfig = *configuration;

	MQTT_State_Set( MQTT_STATE_DISCONNECTED );

	return 0;
}

/*! @brief Connect to an MQTT broker.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Connect( 											\
				struct mqtt_helper_custom_conn_params *connectionParametes )
{
	int err;

	__ASSERT_NO_MSG( connectionParametes != NULL );

	if ( !MQTT_State_Verify( MQTT_STATE_DISCONNECTED ) ) {
		LOG_ERR( "Library is in the wrong state ( %s ), %s required",
			State_Name_Get( MQTT_State_Get(  ) ),
			State_Name_Get( MQTT_STATE_DISCONNECTED ) );

		return -EOPNOTSUPP;
	}

	err = Client_Connect( connectionParametes );
	
	if ( err ) {
		MQTT_State_Set( MQTT_STATE_DISCONNECTED );
		return err;
	}

	LOG_INF( "MQTT connection request sent" );

	k_sem_give( &Connection_Poll_Semaphore );

	return 0;
}

/*! @brief Disconnect from the MQTT broker.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Disconnect( void )
{
	int err;

	if ( !MQTT_State_Verify( MQTT_STATE_CONNECTED ) ) {
		LOG_ERR( "Library is in the wrong state ( %s ), %s required",
			State_Name_Get( MQTT_State_Get(  ) ),
			State_Name_Get( MQTT_STATE_CONNECTED ) );

		return -EOPNOTSUPP;
	}

	MQTT_State_Set( MQTT_STATE_DISCONNECTING );

	err = mqtt_disconnect( &mqttClient );
	if ( err ) {
		/* Treat the sitation as an ungraceful disconnect */
		LOG_ERR( "Failed to send disconnection request, "					\
				"treating as disconnected" );
		MQTT_State_Set( MQTT_STATE_DISCONNECTED );

		if ( currentConfig.cb.on_disconnect ) {
			currentConfig.cb.on_disconnect( err );
		}
	}

	return err;
}

/*! @brief Subscribe to MQTT topics.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Subscribe( 											\
							struct mqtt_subscription_list *subscriptionList )
{
	int err;

	__ASSERT_NO_MSG( subscriptionList != NULL );

	if ( !MQTT_State_Verify( MQTT_STATE_CONNECTED ) ) {
		LOG_ERR( "Library is in the wrong state ( %s ), %s required",
			State_Name_Get( MQTT_State_Get(  ) ),
			State_Name_Get( MQTT_STATE_CONNECTED ) );

		return -EOPNOTSUPP;
	}

	for ( size_t i = 0; i < subscriptionList->list_count; i++ ) {
		LOG_DBG( "Subscribing to: %s", 										\
				( char * )subscriptionList->list[i].topic.utf8 );
	}

	err = mqtt_subscribe( &mqttClient, subscriptionList );
	if ( err ) {
		return err;
	}

	return 0;
}

/*! @brief Publish an MQTT message.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_custom_Publish( const struct mqtt_publish_param *parameters )
{
	LOG_DBG( "Publishing to topic: %.*s",									\
		parameters->message.topic.topic.size,								\
		( char * )parameters->message.topic.topic.utf8 );

	if ( !MQTT_State_Verify( MQTT_STATE_CONNECTED ) ) {
		LOG_ERR( "Library is in the wrong state ( %s ), %s required",		\
			State_Name_Get( MQTT_State_Get(  ) ),							\
			State_Name_Get( MQTT_STATE_CONNECTED ) );

		return -EOPNOTSUPP;
	}

	return mqtt_publish( &mqttClient, parameters );
}

/*! @brief Deinitialize library. Must be called when all MQTT operations are done to
 *	   release resources and allow for a new client. The client must be in a disconnected state.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Deinitializer( void )
{
	if ( !MQTT_State_Verify( MQTT_STATE_DISCONNECTED ) ) {
		LOG_ERR( "Library is in the wrong state ( %s ), %s required",		\
			State_Name_Get( MQTT_State_Get(  ) ),							\
			State_Name_Get( MQTT_STATE_DISCONNECTED ) );

		return -EOPNOTSUPP;
	}

	memset( &currentConfig, 0, sizeof( currentConfig ) );
	memset( &mqttClient, 0, sizeof( mqttClient ) );

	MQTT_State_Set( MQTT_STATE_UNINIT );

	return 0;
}

/*! MQTT_Helper_Custom_Poll_Loop poll loop with the connection process
 *  
 */
MQTT_HELPER_CUSTOM_STATIC void MQTT_Helper_Custom_Poll_Loop( void )
{
	int ret;
	struct pollfd fds[1] = {0};

	LOG_DBG( "Waiting for Connection_Poll_Semaphore" );
	k_sem_take( &Connection_Poll_Semaphore, K_FOREVER );
	LOG_DBG( "Took Connection_Poll_Semaphore" );

	fds[0].events = POLLIN;
#if defined( CONFIG_MQTT_LIB_TLS )
	fds[0].fd  = mqttClient.transport.tls.sock;
#else
	fds[0].fd = mqttClient.transport.tcp.sock;
#endif /* CONFIG_MQTT_LIB_TLS */

	LOG_DBG( "Starting to poll on socket, fd: %d", fds[0].fd );

	while ( true ) {
		if ( !MQTT_State_Verify( MQTT_STATE_CONNECTING ) &&					\
		    !MQTT_State_Verify( MQTT_STATE_CONNECTED ) ) {
			LOG_DBG( "Disconnected on MQTT level, ending poll loop" );
			break;
		} else {
			LOG_DBG( "Polling on socket fd: %d", fds[0].fd );
		}

		ret = poll( 														\
				fds, 														\
				ARRAY_SIZE( fds ), 											\
				mqtt_keepalive_time_left( &mqttClient ) );

		if ( ret < 0 ) {
			LOG_ERR( "poll(  ) returned an error ( %d ), errno: %d", 		\
					ret, -errno );
			break;
		}

		/* If poll returns 0, the timeout has expired. */
		if ( ret == 0 ) {
			ret = mqtt_live( &mqttClient );
			/* -EAGAIN indicates it is not time to ping; try later;
			 * otherwise, connection was closed due to NAT timeout.
			 */
			if ( ret && ( ret != -EAGAIN ) ) {
				LOG_ERR( "Cloud MQTT keepalive ping failed: %d", ret );
				break;
			}
			continue;
		}

		if ( ( fds[0].revents & POLLIN ) == POLLIN ) {
			ret = mqtt_input( &mqttClient );
			if ( ret ) {
				LOG_ERR( "Cloud MQTT input error: %d", ret );
				( void )mqtt_abort( &mqttClient );
				break;
			}

			/* If connection state is set to STATE_DISCONNECTED at
			 * this point we know that the socket has
			 * been closed and we can break out of poll.
			 */
			if ( MQTT_State_Verify( MQTT_STATE_DISCONNECTED ) ||			\
			    MQTT_State_Verify( MQTT_STATE_UNINIT ) ) {
				LOG_DBG( "The socket is already closed" );
				break;
			}
		}

		if ( ( fds[0].revents & POLLNVAL ) == POLLNVAL ) {
			if ( MQTT_State_Verify( MQTT_STATE_DISCONNECTING ) ) {
				/* POLLNVAL is to be expected while
				 * disconnecting, as the socket will be closed
				 * by the MQTT library and become invalid.
				 */
				LOG_DBG( "POLLNVAL while disconnecting" );
			} else if ( MQTT_State_Verify( MQTT_STATE_DISCONNECTED ) ) {
				LOG_DBG( "POLLNVAL, no active connection" );
			} else {
				LOG_ERR( "Socket error: POLLNVAL" );
				LOG_ERR( "The socket was unexpectedly closed" );
			}

			( void )mqtt_abort( &mqttClient );

			break;
		}

		if ( ( fds[0].revents & POLLHUP ) == POLLHUP ) {
			LOG_ERR( "Socket error: POLLHUP" );
			LOG_ERR( "Connection was unexpectedly closed" );
			( void )mqtt_abort( &mqttClient );
			break;
		}

		if ( ( fds[0].revents & POLLERR ) == POLLERR ) {
			LOG_ERR( "Socket error: POLLERR" );
			LOG_ERR( "Connection was unexpectedly closed" );
			( void )mqtt_abort( &mqttClient );
			break;
		}
	}
}

/*! MQTT_Helper_Custom_Run executes the poll loop with the connection process
 * as the main task.
 *  
 */
static void MQTT_Helper_Custom_Run( void )
{
	while ( true ) {
		MQTT_Helper_Custom_Poll_Loop(  );
	}
}

K_THREAD_DEFINE( mqtt_helper_custom_thread, CONFIG_MQTT_HELPER_STACK_SIZE,
		MQTT_Helper_Custom_Run, false, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0 );
