PID Motor Control

This is an in-progress project for PID speed control of a physical system. The goal is to keep the speed of the motor constant despite external torques applied to the shaft. Further on with the project I would like to also work on position control. This will allow specified movements of the motor.

The PID control loop was discretized using the Tustin transform, which can be seen from the difference equation used for the controller algorithm.

## HARDWARE BREAKDOWN
- **MCU**: STM32F0 development board
- **Motor driver**: BTS7960 - 43A motor driver
- **Motor**: Metal DC Geared Motor with Encoder – 12V 83RPM

Currently testing on hardware and refining controller gains on hardware.