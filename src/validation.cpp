#include <vector>
#include <thread>
#include <memory>
#include <atomic>

#include <concurrentqueue/concurrentqueue.h>

#include <vklive/message.h>
#include <vklive/validation.h>

// Shaders that are currently in the validation path
std::vector<fs::path> ValidationCurrentShaders;

// Enable messages since we are in a new run of validation
std::atomic_bool enableValidationMessages = true;

// An error has been triggered during validation
std::atomic_bool validationFoundError = false;

// Thread safe queue of error messages
std::shared_ptr<moodycamel::ConcurrentQueue<Message>> spValidationMessageQueue = std::make_shared<moodycamel::ConcurrentQueue<Message>>();

// Put a limit on validation error reports to stop cascade errors 
static const size_t MaxValidationMessages = 5;

void validation_enable_messages(bool enable)
{
    enableValidationMessages = enable;
}

void validation_set_shaders(const std::vector<fs::path>& shaders)
{
    ValidationCurrentShaders = shaders;
}

void validation_clear_error_state()
{
    validationFoundError = false;
}

bool validation_get_error_state()
{
    return validationFoundError;
}

// Report a validation error and store messages if necessary for files
// Also record the error state
void validation_error(const std::string& text)
{
    if (spValidationMessageQueue->size_approx() < MaxValidationMessages && enableValidationMessages.load())
    {
        Message msg;
        msg.severity = MessageSeverity::Error;
        msg.text = text;
        msg.line = -1;

        if (!ValidationCurrentShaders.empty())
        {
            for (auto& shader : ValidationCurrentShaders)
            {
                msg.path = shader;
                spValidationMessageQueue->enqueue(msg);
            }
        }
        else
        {
            spValidationMessageQueue->enqueue(msg);
        }
    }
    // Signal the error
    validationFoundError = true;
}

bool validation_check_message_queue(Message& msg)
{
    return spValidationMessageQueue->try_dequeue(msg);
}
