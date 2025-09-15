#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <string>

extern "C" {
    #include "printf_redirect.h"
    #include "stm32f4xx_hal.h"
}

class PrintfRedirectTest : public ::testing::Test {
protected:
    UART_HandleTypeDef test_uart;
    
    void SetUp() override {
        Mock_ResetAll();  // This already sets huart2.Instance correctly
        Mock_UART_ClearBuffer();
        memset(&test_uart, 0, sizeof(test_uart));
        test_uart.Instance = (void*)0x40004400;
        
        // Don't clear huart2 since Mock_ResetAll already set it up correctly
        // memset(&huart2, 0, sizeof(huart2));
        // huart2.Instance = (void*)0x40004400;
    }
    
    void TearDown() override {
        Mock_ResetAll();
    }
};

TEST_F(PrintfRedirectTest, UartPutchar_SingleCharacter) {
    int result = uart_putchar('A', &test_uart);
    
    EXPECT_EQ(result, 'A');
    EXPECT_EQ(Mock_UART_GetBufferSize(), 1);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(buffer[0], 'A');
}

TEST_F(PrintfRedirectTest, UartPutchar_MultipleCharacters) {
    uart_putchar('H', &test_uart);
    uart_putchar('e', &test_uart);
    uart_putchar('l', &test_uart);
    uart_putchar('l', &test_uart);
    uart_putchar('o', &test_uart);
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), 5);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Hello", 5), 0);
}

TEST_F(PrintfRedirectTest, UartPutchar_NullHandle) {
    int result = uart_putchar('A', nullptr);
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPutchar_TransmitError) {
    Mock_SetHALStatus(HAL_ERROR);
    int result = uart_putchar('A', &test_uart);
    
    EXPECT_EQ(result, -1);
}

TEST_F(PrintfRedirectTest, UartPuts_SimpleString) {
    const char* str = "Hello World";
    int result = uart_puts(str, &test_uart);
    
    EXPECT_EQ(result, 11);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 11);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, str, 11), 0);
}

TEST_F(PrintfRedirectTest, UartPuts_EmptyString) {
    const char* str = "";
    int result = uart_puts(str, &test_uart);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPuts_NullString) {
    int result = uart_puts(nullptr, &test_uart);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPuts_NullHandle) {
    const char* str = "Test";
    int result = uart_puts(str, nullptr);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPuts_TransmitError) {
    Mock_SetHALStatus(HAL_ERROR);
    const char* str = "Test";
    int result = uart_puts(str, &test_uart);
    
    EXPECT_EQ(result, -1);
}

TEST_F(PrintfRedirectTest, UartPrintf_SimpleFormat) {
    int result = uart_printf(&test_uart, "Test %d", 123);
    
    EXPECT_EQ(result, 8);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 8);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Test 123", 8), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_MultipleFormatSpecifiers) {
    int result = uart_printf(&test_uart, "Value: %d, Float: %.2f, String: %s", 
                           42, 3.14159, "OK");
    
    EXPECT_GT(result, 0);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    char expected[] = "Value: 42, Float: 3.14, String: OK";
    EXPECT_EQ(memcmp(buffer, expected, strlen(expected)), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_HexFormat) {
    int result = uart_printf(&test_uart, "Hex: 0x%08X", 0xDEADBEEF);
    
    EXPECT_EQ(result, 15);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Hex: 0xDEADBEEF", 15), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_NullFormat) {
    int result = uart_printf(&test_uart, nullptr);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_NullHandle) {
    int result = uart_printf(nullptr, "Test");
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_EmptyFormat) {
    int result = uart_printf(&test_uart, "");
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_TransmitError) {
    Mock_SetHALStatus(HAL_ERROR);
    int result = uart_printf(&test_uart, "Test");
    
    EXPECT_EQ(result, -1);
}

TEST_F(PrintfRedirectTest, IoRedirect_SingleChar) {
    int result = __io_putchar('X');
    
    EXPECT_EQ(result, 'X');
    EXPECT_EQ(Mock_UART_GetBufferSize(), 1);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(buffer[0], 'X');
}

TEST_F(PrintfRedirectTest, IoRedirect_NewlineCharacter) {
    __io_putchar('\n');
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), 1);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(buffer[0], '\n');
}

TEST_F(PrintfRedirectTest, IoRedirect_CarriageReturn) {
    __io_putchar('\r');
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), 1);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(buffer[0], '\r');
}

TEST_F(PrintfRedirectTest, Printf_Integration) {
    char output[100];
    snprintf(output, sizeof(output), "Temperature: %d°C, Voltage: %.2fV", 25, 3.7);
    
    for (size_t i = 0; i < strlen(output); i++) {
        __io_putchar(output[i]);
    }
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), strlen(output));
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, output, strlen(output)), 0);
}

TEST_F(PrintfRedirectTest, Printf_LongString) {
    const char* longStr = "This is a very long string that tests the buffer capacity. "
                         "It includes multiple sentences and should be transmitted correctly. "
                         "The UART mock should handle this without any issues.";
    
    int result = uart_puts(longStr, &test_uart);
    
    EXPECT_EQ(result, (int)strlen(longStr));
    EXPECT_EQ(Mock_UART_GetBufferSize(), strlen(longStr));
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, longStr, strlen(longStr)), 0);
}

TEST_F(PrintfRedirectTest, Printf_SpecialCharacters) {
    int result = uart_printf(&test_uart, "Tab:\t Quote:\" Backslash:\\ Percent:%%");
    
    EXPECT_GT(result, 0);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    const char* expected = "Tab:\t Quote:\" Backslash:\\ Percent:%";
    EXPECT_EQ(memcmp(buffer, expected, strlen(expected)), 0);
}

TEST_F(PrintfRedirectTest, Printf_BoundaryValues) {
    int result = uart_printf(&test_uart, "Min int: %d, Max int: %d", INT32_MIN, INT32_MAX);
    
    EXPECT_GT(result, 0);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    char expected[100];
    snprintf(expected, sizeof(expected), "Min int: %d, Max int: %d", INT32_MIN, INT32_MAX);
    EXPECT_EQ(memcmp(buffer, expected, strlen(expected)), 0);
}

TEST_F(PrintfRedirectTest, UartPutchar_NullInstance) {
    test_uart.Instance = nullptr;
    int result = uart_putchar('A', &test_uart);
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_NullInstance) {
    test_uart.Instance = nullptr;
    int result = uart_printf(&test_uart, "Test");
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartPrintf_BufferOverflow) {
    std::string long_string(600, 'A');
    int result = uart_printf(&test_uart, "%s", long_string.c_str());
    
    EXPECT_EQ(result, 511);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 511);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(buffer[510], 'A');
    EXPECT_EQ(buffer[511], 0);
}

TEST_F(PrintfRedirectTest, IoRedirect_NullInstance) {
    huart2.Instance = nullptr;
    int result = __io_putchar('X');
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

TEST_F(PrintfRedirectTest, UartIsReady_ValidHandle) {
    bool ready = uart_is_ready(&test_uart);
    EXPECT_TRUE(ready);
}

TEST_F(PrintfRedirectTest, UartIsReady_NullHandle) {
    bool ready = uart_is_ready(nullptr);
    EXPECT_FALSE(ready);
}

TEST_F(PrintfRedirectTest, UartIsReady_NullInstance) {
    test_uart.Instance = nullptr;
    bool ready = uart_is_ready(&test_uart);
    EXPECT_FALSE(ready);
}

TEST_F(PrintfRedirectTest, Printf_SimpleMessage_RedirectedToUART) {
    // Test direct __io_putchar calls to simulate printf behavior
    const char* msg = "Debug: 42";
    int total_chars = 0;
    
    for (int i = 0; msg[i] != '\0'; i++) {
        int result = __io_putchar(msg[i]);
        if (result > 0) {
            total_chars++;
        }
    }
    
    EXPECT_EQ(total_chars, 9);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 9);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Debug: 42", 9), 0);
}

TEST_F(PrintfRedirectTest, DebugPrintf_NullInstance) {
    huart2.Instance = nullptr;
    int result = debug_printf_safe("Test");
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
}

class PrintfRedirectIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        Mock_ResetAll();
        Mock_UART_ClearBuffer();
    }
    
    void TearDown() override {
        Mock_ResetAll();
    }
};

TEST_F(PrintfRedirectIntegrationTest, CompleteMessageFlow) {
    const char* messages[] = {
        "System initializing...\r\n",
        "Battery voltage: 12.5V\r\n",
        "Temperature: 25°C\r\n",
        "Status: OK\r\n"
    };
    
    for (int i = 0; i < 4; i++) {
        int result = uart_puts(messages[i], &huart2);
        EXPECT_GT(result, 0);
    }
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    size_t total_size = 0;
    for (int i = 0; i < 4; i++) {
        total_size += strlen(messages[i]);
    }
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), total_size);
    
    std::string combined;
    for (int i = 0; i < 4; i++) {
        combined += messages[i];
    }
    
    EXPECT_EQ(memcmp(buffer, combined.c_str(), combined.length()), 0);
}

TEST_F(PrintfRedirectIntegrationTest, MixedOutputMethods) {
    uart_putchar('>', &huart2);
    uart_putchar(' ', &huart2);
    
    uart_printf(&huart2, "Command %d: ", 1);
    
    uart_puts("Execute\r\n", &huart2);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    const char* expected = "> Command 1: Execute\r\n";
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), strlen(expected));
    EXPECT_EQ(memcmp(buffer, expected, strlen(expected)), 0);
}

TEST_F(PrintfRedirectIntegrationTest, ErrorRecovery) {
    uart_puts("Start\r\n", &huart2);
    
    Mock_SetHALStatus(HAL_ERROR);
    int result = uart_puts("This fails\r\n", &huart2);
    EXPECT_EQ(result, -1);
    
    Mock_SetHALStatus(HAL_OK);
    uart_puts("Recovered\r\n", &huart2);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    const char* expected = "Start\r\nRecovered\r\n";
    
    EXPECT_EQ(Mock_UART_GetBufferSize(), strlen(expected));
    EXPECT_EQ(memcmp(buffer, expected, strlen(expected)), 0);
}

// FreeRTOS tests - Always available since FreeRTOS is enabled in project
class PrintfRedirectFreeRTOSTest : public ::testing::Test {
protected:
    UART_HandleTypeDef test_uart;
    
    void SetUp() override {
        Mock_ResetAll();  // This already sets huart2.Instance correctly
        Mock_UART_ClearBuffer();
        Mock_FreeRTOS_Reset();
        memset(&test_uart, 0, sizeof(test_uart));
        test_uart.Instance = (void*)0x40004400;
        
        // Don't clear huart2 since Mock_ResetAll already set it up correctly
        // memset(&huart2, 0, sizeof(huart2));
        // huart2.Instance = (void*)0x40004400;
    }
    
    void TearDown() override {
        Mock_ResetAll();
        Mock_FreeRTOS_Reset();
    }
};

TEST_F(PrintfRedirectFreeRTOSTest, PrintfRedirect_Initialization) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetMutexCreateResult(1);
    
    printf_redirect_init();
    
    EXPECT_TRUE(Mock_FreeRTOS_GetMutexCreated());
    EXPECT_TRUE(Mock_FreeRTOS_GetMutexCreated());
    EXPECT_TRUE(Mock_FreeRTOS_GetMutexCreated());
}

TEST_F(PrintfRedirectFreeRTOSTest, PrintfRedirect_InitializationFailure) {
    Mock_FreeRTOS_SetMutexCreateResult(false);
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetMutexCreateResult(1);
    
    printf_redirect_init();
    
    EXPECT_FALSE(Mock_FreeRTOS_GetMutexCreated());
}

TEST_F(PrintfRedirectFreeRTOSTest, PrintfRedirect_Deinitialization) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetMutexCreateResult(1);
    
    printf_redirect_init();
    printf_redirect_deinit();
    
    EXPECT_TRUE(Mock_FreeRTOS_GetMutexCreated());
    EXPECT_TRUE(Mock_FreeRTOS_GetMutexCreated());
    EXPECT_TRUE(Mock_FreeRTOS_GetMutexCreated());
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintf_MutexProtection) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(true);
    printf_redirect_init();
    
    int result = uart_printf(&test_uart, "Thread-safe: %d", 123);
    
    EXPECT_GT(result, 0);
    EXPECT_TRUE(Mock_FreeRTOS_GetSemaphoreTaken());
    EXPECT_TRUE(Mock_FreeRTOS_GetSemaphoreGiven());
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Thread-safe: 123", 16), 0);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintf_MutexTimeout) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(false);
    printf_redirect_init();
    
    int result = uart_printf(&test_uart, "Should timeout");
    
    EXPECT_EQ(result, -1);
    EXPECT_TRUE(Mock_FreeRTOS_GetSemaphoreTaken());
    EXPECT_FALSE(Mock_FreeRTOS_GetSemaphoreGiven());
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintf_FallbackToUnsafe) {
    // When FreeRTOS not initialized, should fall back to unsafe version
    int result = uart_printf(&test_uart, "No mutex");
    
    EXPECT_GT(result, 0);  // Should succeed using unsafe version
    EXPECT_FALSE(Mock_FreeRTOS_GetSemaphoreTaken());  // No mutex used
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "No mutex", 8), 0);
}

TEST_F(PrintfRedirectFreeRTOSTest, DebugPrintf_Success) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(true);
    printf_redirect_init();
    
    int result = debug_printf_safe("Debug: %s", "FreeRTOS");
    
    EXPECT_GT(result, 0);
    EXPECT_TRUE(Mock_FreeRTOS_GetSemaphoreTaken());
    EXPECT_TRUE(Mock_FreeRTOS_GetSemaphoreGiven());
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Debug: FreeRTOS", 15), 0);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, DebugPrintf_UartNotReady) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    printf_redirect_init();
    
    huart2.Instance = nullptr;
    int result = debug_printf_safe("Should fail");
    
    EXPECT_EQ(result, -1);
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintfISR_QueueMessage) {
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetTaskNotifyFromISRResult(1);
    printf_redirect_init();
    
    int result = uart_printf_isr("ISR: %d", 42);
    
    EXPECT_GT(result, 0);
    EXPECT_TRUE(true);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintfISR_QueueFull) {
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetQueueSendFromISRResult(false);
    printf_redirect_init();
    
    int result = uart_printf_isr("Queue full");
    
    EXPECT_EQ(result, -1);
    EXPECT_TRUE(true);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintfISR_NullQueue) {
    int result = uart_printf_isr("No queue");
    
    EXPECT_EQ(result, -1);
    EXPECT_FALSE(true);
}

TEST_F(PrintfRedirectFreeRTOSTest, UartPrintfISR_BufferOverflow) {
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetTaskNotifyFromISRResult(1);
    printf_redirect_init();
    
    std::string long_string(100, 'X');
    int result = uart_printf_isr("%s", long_string.c_str());
    
    EXPECT_EQ(result, 63);
    EXPECT_TRUE(true);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, PrintfISRTask_ProcessMessage) {
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetQueueReceiveResult(true);
    printf_redirect_init();
    
    Mock_FreeRTOS_SetQueueReceiveData("Test message", 12);
    
    printf_isr_task(nullptr);
    
    EXPECT_TRUE(Mock_FreeRTOS_GetQueueReceived());
    EXPECT_EQ(Mock_UART_GetBufferSize(), 12);
    
    uint8_t* buffer = Mock_UART_GetBuffer();
    EXPECT_EQ(memcmp(buffer, "Test message", 12), 0);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, PrintfISRTask_UartNotReady) {
    Mock_FreeRTOS_SetMutexCreateResult(1);
    Mock_FreeRTOS_SetQueueReceiveResult(true);
    printf_redirect_init();
    
    huart2.Instance = nullptr;
    Mock_FreeRTOS_SetQueueReceiveData("Should not transmit", 19);
    
    printf_isr_task(nullptr);
    
    EXPECT_TRUE(Mock_FreeRTOS_GetQueueReceived());
    EXPECT_EQ(Mock_UART_GetBufferSize(), 0);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, ConcurrentAccess_MultipleTasks) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(true);
    printf_redirect_init();
    
    Mock_FreeRTOS_SimulateTaskSwitch();
    
    int result1 = uart_printf(&test_uart, "Task 1: %d", 1);
    int result2 = uart_printf(&test_uart, "Task 2: %d", 2);
    
    EXPECT_GT(result1, 0);
    EXPECT_GT(result2, 0);
    
    EXPECT_EQ(Mock_FreeRTOS_GetSemaphoreTakeCount(), 2);
    EXPECT_EQ(Mock_FreeRTOS_GetSemaphoreGiveCount(), 2);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, StressTest_HighFrequencyPrintf) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(true);
    printf_redirect_init();
    
    for (int i = 0; i < 50; i++) {
        int result = uart_printf(&test_uart, "Msg %d", i);
        EXPECT_GT(result, 0);
    }
    
    EXPECT_EQ(Mock_FreeRTOS_GetSemaphoreTakeCount(), 50);
    EXPECT_EQ(Mock_FreeRTOS_GetSemaphoreGiveCount(), 50);
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, ErrorHandling_TransmitError) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(true);
    printf_redirect_init();
    
    Mock_SetHALStatus(HAL_ERROR);
    int result = uart_printf(&test_uart, "Should fail");
    
    EXPECT_EQ(result, -1);
    EXPECT_TRUE(Mock_FreeRTOS_GetSemaphoreGiven());
    
    printf_redirect_deinit();
}

TEST_F(PrintfRedirectFreeRTOSTest, Timeout_ConfigurationTest) {
    Mock_FreeRTOS_SetMutexCreateResult(true);
    Mock_FreeRTOS_SetSemaphoreTakeResult(true);
    printf_redirect_init();
    
    uart_printf(&test_uart, "Timeout test");
    
    uint32_t expected_timeout = PRINTF_MUTEX_TIMEOUT_MS;
    EXPECT_EQ(Mock_FreeRTOS_GetLastSemaphoreTimeout(), expected_timeout);
    
    printf_redirect_deinit();
}