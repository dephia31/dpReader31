/*
 * 中文：只观察堆与任务栈，不更改电源或存储。/ English: Observe heaps and task stacks without changing power/storage.
 */
#pragma once
/// 从普通任务调用；stage不得包含凭据。/ Call from a normal task; stage must not contain credentials.
void print_memory_profile(const char *stage);
/// 一次性PSRAM快照；不在ISR/critical/cache关闭时调用。/ One PSRAM snapshot; never call from ISR/critical/cache-off context.
void print_task_stack_profile(void);
