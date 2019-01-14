#include "legato.h"
#include "interfaces.h"
#include <jansson.h>


static void SetBootSource(void)
{
    char reason[32] = {'\0'};

    if (le_bootReason_WasTimer())
    {
        strcpy(reason, "timer");
    }
    else if (le_bootReason_WasGpio(36))
    {
        strcpy(reason, "gpio36");
    }
    else if (le_bootReason_WasGpio(38))
    {
        strcpy(reason, "gpio38");
    }
    else if (le_bootReason_WasAdc(2))
    {
        strcpy(reason, "adc2");
    }
    else if (le_bootReason_WasAdc(3))
    {
        strcpy(reason, "adc");
    }

    io_PushString(
        "BootReason",
        IO_NOW,
        reason);
}

static void ShutdownPushHandler(double timestamp, void *context)
{
    LE_DEBUG("Handling Shutdown trigger");
    le_result_t res = le_ulpm_ShutDown();
    if (res != LE_OK)
    {
        LE_WARN("Call to le_ulpm_ShutDown failed - %s", LE_RESULT_TXT(res));
    }
}

static void GpioConfigPushHandler(double timestamp, const char *jsonStr, void *context)
{
    /*
     * Expect jsonStr in the form:
     * { "gpioNum": 36, "gpioState": "GPIO_RISING" }
     */
    json_error_t loadError;
    const size_t loadFlags = 0;
    json_t *json = json_loads(jsonStr, loadFlags, &loadError);
    if (json == NULL)
    {
        LE_ERROR("Received invalid JSON: \"%s\". Parsing gave error: %s", jsonStr, loadError.text);
        return;
    }

    int gpioNumSigned;
    const char *gpioStateStr;
    const int unpackRes = json_unpack(
        json,
        "{s:i,s:s}",
        "gpioNum", &gpioNumSigned,
        "gpioState", &gpioStateStr);
    if (unpackRes)
    {
        LE_ERROR("Valid JSON has unexpected format: \"%s\"", jsonStr);
        goto cleanup;
    }

    if (gpioNumSigned < 0)
    {
        LE_ERROR("gpioNum was %d, and should be >= 0", gpioNumSigned);
        goto cleanup;
    }
    uint32_t gpioNum = gpioNumSigned;

    le_ulpm_GpioState_t gpioState;
    if (strcmp(gpioStateStr, "GPIO_LOW") == 0)
    {
        gpioState = LE_ULPM_GPIO_LOW;
    }
    else if (strcmp(gpioStateStr, "GPIO_HIGH") == 0)
    {
        gpioState = LE_ULPM_GPIO_HIGH;
    }
    else if (strcmp(gpioStateStr, "GPIO_RISING") == 0)
    {
        gpioState = LE_ULPM_GPIO_RISING;
    }
    else if (strcmp(gpioStateStr, "GPIO_FALLING") == 0)
    {
        gpioState = LE_ULPM_GPIO_FALLING;
    }
    else if (strcmp(gpioStateStr, "GPIO_BOTH") == 0)
    {
        gpioState = LE_ULPM_GPIO_BOTH;
    }
    else if (strcmp(gpioStateStr, "GPIO_OFF") == 0)
    {
        gpioState = LE_ULPM_GPIO_OFF;
    }
    else
    {
        LE_ERROR("Invalid gpioState \"%s\"", gpioStateStr);
        goto cleanup;
    }

    le_result_t res = le_ulpm_BootOnGpio(gpioNum, gpioState);
    LE_WARN_IF(
        res != LE_OK,
        "Failed when attempting to set boot gpio=%d, state=%d - %s",
        gpioNum,
        gpioState,
        LE_RESULT_TXT(res));

cleanup:
    json_decref(json);
}

static void TimerConfigPushHandler(double timestamp, double timeInSeconds, void *context)
{
    le_result_t res = le_ulpm_BootOnTimer(timeInSeconds);
    LE_WARN_IF(
        res != LE_OK,
        "Failed when attempting to set boot timer to %f - %s",
        timeInSeconds,
        LE_RESULT_TXT(res));
}

static void AdcConfigPushHandler(double timestamp, const char *jsonStr, void *context)
{
    /*
     * Expect jsonStr in the form:
     * {
     *   "adcNum": 2,
     *   "pollIntervalInMs": 1000,
     *   "bootAboveAdcReading": 200.0,
     *   "bootBelowAdcReading": 1000.0
     * }
     */
    json_error_t loadError;
    const size_t loadFlags = 0;
    json_t *json = json_loads(jsonStr, loadFlags, &loadError);
    if (json == NULL)
    {
        LE_ERROR("Received invalid JSON: \"%s\". Parsing gave error: %s", jsonStr, loadError.text);
        return;
    }

    int adcNumSigned;
    int pollIntervalInMsSigned;
    double bootAboveAdcReading;
    double bootBelowAdcReading;
    const int unpackRes = json_unpack(
        json,
        "{s:i,s:i,s:f,s:f}",
        "adcNum", &adcNumSigned,
        "pollIntervalInMs", &pollIntervalInMsSigned,
        "bootAboveAdcReading", &bootAboveAdcReading,
        "bootBelowAdcReading", &bootBelowAdcReading);
    if (unpackRes)
    {
        LE_ERROR("Valid JSON has unexpected format: \"%s\"", jsonStr);
        goto cleanup;
    }

    if (pollIntervalInMsSigned <= 0)
    {
        LE_ERROR("pollIntervalInMs was %d, and should be > 0", pollIntervalInMsSigned);
        goto cleanup;
    }
    uint32_t pollIntervalInMs = pollIntervalInMsSigned;

    if (adcNumSigned < 0)
    {
        LE_ERROR("adcNum was %d, and should be > 0", adcNumSigned);
        goto cleanup;
    }
    uint32_t adcNum = adcNumSigned;

    le_result_t res = le_ulpm_BootOnAdc(
        adcNum, pollIntervalInMs, bootAboveAdcReading, bootBelowAdcReading);
    LE_WARN_IF(
        res != LE_OK,
        "Failed when attempting to set boot adc=%d, poll=%d ms, bootAboveReading=%f, bootBelowReading=%f - %s",
        adcNum,
        pollIntervalInMs,
        bootAboveAdcReading,
        bootBelowAdcReading,
        LE_RESULT_TXT(res));

cleanup:
    json_decref(json);
}


COMPONENT_INIT
{
    /*
     * le_bootReason.api
     */
    le_result_t res = io_CreateInput("BootReason", IO_DATA_TYPE_STRING, "");
    LE_FATAL_IF(res != LE_OK, "Couldn't create BootReason data hub input - %s", LE_RESULT_TXT(res));
    SetBootSource();

    /*
     * le_ulpm.api
     */
    res = io_CreateInput("FirmwareVersion", IO_DATA_TYPE_STRING, "");
    LE_FATAL_IF(
        res != LE_OK, "Couldn't create FirmwareVersion data hub input - %s", LE_RESULT_TXT(res));
    char firmwareVersion[LE_ULPM_MAX_VERS_LEN + 1];
    res = le_ulpm_GetFirmwareVersion(firmwareVersion, sizeof(firmwareVersion));
    LE_FATAL_IF(res != LE_OK, "Couldn't lookup MCU firmware version - %s", LE_RESULT_TXT(res));
    io_PushString(
        "FirmwareVersion",
        IO_NOW,
        firmwareVersion);

    res = io_CreateOutput("Shutdown", IO_DATA_TYPE_TRIGGER, "");
    LE_FATAL_IF(res != LE_OK, "Couldn't create Shutdown data hub output - %s", LE_RESULT_TXT(res));
    io_MarkOptional("Shutdown");
    io_AddTriggerPushHandler("Shutdown", ShutdownPushHandler, NULL);

    res = io_CreateOutput("GpioConfig", IO_DATA_TYPE_JSON, "");
    LE_FATAL_IF(res != LE_OK, "Couldn't create GpioConfig data hub output - %s", LE_RESULT_TXT(res));
    io_MarkOptional("GpioConfig");
    io_AddJsonPushHandler("GpioConfig", GpioConfigPushHandler, NULL);

    res = io_CreateOutput("TimerConfig", IO_DATA_TYPE_NUMERIC, "s");
    LE_FATAL_IF(res != LE_OK, "Couldn't create TimerConfig data hub output - %s", LE_RESULT_TXT(res));
    io_MarkOptional("TimerConfig");
    io_AddNumericPushHandler("TimerConfig", TimerConfigPushHandler, NULL);

    res = io_CreateOutput("AdcConfig", IO_DATA_TYPE_JSON, "");
    LE_FATAL_IF(res != LE_OK, "Couldn't create AdcConfig data hub output - %s", LE_RESULT_TXT(res));
    io_MarkOptional("AdcConfig");
    io_AddJsonPushHandler("AdcConfig", AdcConfigPushHandler, NULL);
}
