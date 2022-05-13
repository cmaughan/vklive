#pragma once

struct Message;

void validation_set_shaders(const std::vector<fs::path>& shaders);
void validation_enable_messages(bool enable);
void validation_clear_error_state();
bool validation_get_error_state();
void validation_error(const std::string& msg);
bool validation_check_message_queue(Message& msg);
void validation_enable_messages(bool enable);
