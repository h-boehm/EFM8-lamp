## lamp

An embedded C program which controls a dual-channel LED array using an EFM8 microcontroller and pulse width modulation, compiled with the Keil toolchain. It interfaces with hardware via five tactile buttons, allowing users to toggle lamp states (on/off), switch between cool and warm channels, and adjust brightness. The software includes a state machine with dedicated states for idle, debouncing, and other button interactions to manage both user inputs and lamp behavior. It also incorporates a debouncing algorithm to eliminate button input noise and typematic rate and delay mechanisms for smooth brightness adjustments.
