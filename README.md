# Data Hub Power Bridge

## What is it?
Exposes the `le_ulpm.api` and `le_bootSource.api` Legato APIs over the Data Hub application.

## Data Hub Inputs
1. `/app/dhubPowerBridge/BootReason` - A string that indicates what caused the device to boot. `""`,
   `"timer"`, `"gpio36"`, `"gpio38"`, `"adc2"` and `"adc3"` are values that could be observed.
1. `/app/dhubPowerBridge/FirmwareVersion` - The lower power MCU firmware version string. An example
   is `"002.011"`.

## Data Hub Outputs
1. `/app/dhubPowerBridge/Shutdown` - A trigger instructing the device to attempt to enter ULPM.
   Failure to enter ULPM will only be visible in logs.
1. `/app/dhubPowerBridge/GpioConfig` - A JSON value of the form
   `{"gpioNum": 36, "gpioState": "GPIO_RISING"}` that configures the GPIO and condition to be used
   for wakeup.
1. `/app/dhubPowerBridge/TimerConfig` - A numeric time in seconds specifying how long to wait in low
   power mode before waking up.
1. `/app/dhubPowerBridge/AdcConfig` - A JSON value of the form
   `{"adcNum": 2, "pollIntervalInMs": 1000, "bootAboveAdcReading": 200.0, "bootBelowAdcReading": 1000.0}`
   that configures an ADC to be used for wakeup.

