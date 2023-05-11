/*! @file MQTT_helper_costum.h
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

#ifndef MQTT_HELPER_CUSTOM__
#define MQTT_HELPER_CUSTOM__

#include <stdio.h>
#include <zephyr/net/mqtt.h>

/*! Possible MQTT connection status */
enum mqtt_state {
	//! MQTT Uninitialized (default state)
	MQTT_STATE_UNINIT,
	//! MQTT disconnected
	MQTT_STATE_DISCONNECTED,
	//! MQTT Transport level connecting
	MQTT_STATE_TRANSPORT_CONNECTING,
	//! MQTT Application level connecting
	MQTT_STATE_CONNECTING,
	//! MQTT Transport level connected
	MQTT_STATE_TRANSPORT_CONNECTED,
	//! MQTT Application level connected
	MQTT_STATE_CONNECTED,
	//! MQTT disconnected
	MQTT_STATE_DISCONNECTING,
	//! MQTT count
	MQTT_STATE_COUNT,
};

/*! Possible MQTT_Helper_Custom errors */
enum mqtt_helper_custom_error {
	//! The received payload is larger than the payload buffer. 
	MQTT_HELPER_ERROR_MSG_SIZE,
};

/*! @struct mqtt_helper_custom_buf
* @brief struct containing a pointer to a buffer and its size.
*/
struct mqtt_helper_custom_buf {
	/** Pointer to buffer. */
	char *ptr;

	/** Size of buffer. */
	size_t size;
};

/*! @typedef mqtt_helper_custom_handler_t
* @brief handler event's type
*/
typedef void (*mqtt_helper_custom_handler_t)(struct mqtt_evt *evt);
/*! @typedef mqtt_helper_custom_on_connack_t
* @brief type related to the connack callback
*/
typedef void (*mqtt_helper_custom_on_connack_t)								\
			(enum mqtt_conn_return_code return_code);
/*! @typedef mqtt_helper_custom_on_disconnect_t
* @brief type related to the disconnect callback
*/
typedef void (*mqtt_helper_custom_on_disconnect_t)(int result);
/*! @typedef mqtt_helper_custom_on_publish_t
* @brief type related to the publish callback
*/
typedef void (*mqtt_helper_custom_on_publish_t)								\
			(struct mqtt_helper_custom_buf topic_buf,						\
			struct mqtt_helper_custom_buf payload_buf);
typedef void (*mqtt_helper_custom_on_disconnect_t)(int result);
/*! @typedef mqtt_helper_custom_on_puback_t
* @brief type related to the puback callback
*/
typedef void (*mqtt_helper_custom_on_puback_t)								\
			(uint16_t message_id, int result);
/*! @typedef mqtt_helper_custom_on_suback_t
* @brief type related to the suback callback
*/
typedef void (*mqtt_helper_custom_on_suback_t)								\
			(uint16_t message_id, int result);
/*! @typedef mqtt_helper_custom_on_pingresp_t
* @brief type related to the ping response callback
*/
typedef void (*mqtt_helper_custom_on_pingresp_t)(void);
/*! @typedef mqtt_helper_custom_on_pingresp_t
* @brief type related to the error callback
*/
typedef void (*mqtt_helper_custom_on_error_t)								\
			(enum mqtt_helper_custom_error error);


/*! @struct mqtt_helper_custom_cfg
* @brief struct containing all the connection callbacks
*/
struct mqtt_helper_custom_cfg {
	struct {
		mqtt_helper_custom_on_connack_t 		on_connack;
		mqtt_helper_custom_on_disconnect_t 		on_disconnect;
		mqtt_helper_custom_on_publish_t 		on_publish;
		mqtt_helper_custom_on_puback_t 			on_puback;
		mqtt_helper_custom_on_suback_t 			on_suback;
		mqtt_helper_custom_on_pingresp_t		on_pingresp;
		mqtt_helper_custom_on_error_t 			on_error;
	} cb;
};

/*! @struct mqtt_helper_custom_conn_params
* @brief struct containing the connection parameters
*/
struct mqtt_helper_custom_conn_params {
	/* The hostname must be null-terminated. */
	struct mqtt_helper_custom_buf 				hostname;
	struct mqtt_helper_custom_buf 				device_id;
	struct mqtt_helper_custom_buf 				user_name;
};

/*! @brief Initialize the MQTT helper.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Initializer( 										\
								struct mqtt_helper_custom_cfg *configuration );


/*! @brief Connect to an MQTT broker.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Connect( 											\
				struct mqtt_helper_custom_conn_params *connectionParametes );

/*! @brief Disconnect from the MQTT broker.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Disconnect(void);

/*! @brief Subscribe to MQTT topics.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Subscribe( 											\
							struct mqtt_subscription_list *subscriptionList );
/*! @brief Publish an MQTT message.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_custom_Publish( const struct mqtt_publish_param *parameters );

/*! @brief Deinitialize library. Must be called when all MQTT operations are 
 *		done to release resources and allow for a new client. The client must 
 *		be in a disconnected state.
 *
 *  @retval 0 if successful.
 *  @retval -EOPNOTSUPP if operation is not supported in the current state.
 *  @return Otherwise a negative error code.
 */
int MQTT_Helper_Custom_Deinitializer( void );

#endif /* MQTT_HELPER_CUSTOM__ */
