PID Motor Control

This is designed to be implemented on an STM32F0.
Motor driver used:BTS7960 - 43A motor driver
Motor used: Metal DC Geared Motor with Encoder – 12V 83RPM

The PID control loop was discretized using the Tunstin transform, hence the u(k-2) in the difference equation.
The tuning of the PID was done experimentally on the physical system.
