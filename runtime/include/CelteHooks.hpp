#pragma once
#include <functional>
#include <kafka/KafkaConsumer.h>
#include <string>

namespace celte {
    namespace api {

        /**
         * @brief To allow maximum customization of celte's behaviors, most actions taken by celte can be
         * coupled with user defined hooks.
         *
         * The general idea is that for each Celte event cev, there are several hooks:
         *  - pre_cev: called before the event cev is processed by celte.
         *  - post_cev: called after the event cev is processed by celte.
         *  - on_error_cev: called if an error occurs while processing the event cev.
         *
         * pre_cev should return a boolean which, if false, will prevent the event cev from being processed and
         * abord the procedure.
         */
        class HooksTable {
            public:
                /**
                 * @brief Singleton pattern for the HooksTable.
                 */
                static HooksTable& HOOKS();

#ifdef CELTE_SERVER_MODE_ENABLED
                struct {
                    struct {
                        std::function<bool(kafka::clients::consumer::ConsumerRecord)> pre;
                        std::function<void(kafka::clients::consumer::ConsumerRecord)> post;
                        std::function<void(kafka::clients::consumer::ConsumerRecord)> onError;
                    } onSpawnRequest;
                } server;
#else
                struct {
                    struct {
                        std::function<bool(std::string)> pre;
                        std::function<void(std::string)> post;
                        std::function<void(std::string)> onError;
                    } onSpawnAuthorization;
                } client;
#endif
        };
    } // namespace api
} // namespace celte