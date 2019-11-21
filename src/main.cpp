#include <iostream>
#include <iomanip>
#include <fstream>
#include <string.h> 
#include <signal.h>
#include "unicorn.h"
#include "lsl_cpp.h"

// Configuration
#define DEVICE_SERIAL			"UN-2019.02.90"	// The device serial number
#define FRAME_LENGTH			1				// The number of samples acquired per get data call
#define TESTSIGNAL_ENABLED		false			// Flag to enable or disable testsignal
#define STREAM_NAME				"Unicorn"		// The LSL stream name
#define STREAM_TYPE				"EEG"			// The LSL stream type

// Function declarations
void HandleError(int errorCode);
void PrintErrorMessage(int errorCode);
void OnClose(int signal);

// Interrupt signal
volatile sig_atomic_t stop;

// Entry point
int main(int argc, char** argv) {

	// Variable to store error codes
	int errorCode = UNICORN_ERROR_SUCCESS;

	// Structure that holds the handle for the current session
	UNICORN_HANDLE deviceHandle = 0;

	try {

		// Get number of available devices.
		std::cout << "Scanning..." << std::endl << std::endl;
		unsigned int availableDevicesCount = 0;
		errorCode = UNICORN_GetAvailableDevices(NULL, &availableDevicesCount, TRUE);
		HandleError(errorCode);

		if (availableDevicesCount < 1) {
			std::cout << "No device available" << std::endl;
			errorCode = UNICORN_ERROR_GENERAL_ERROR;
			return 1;
		}

		// Get available device serials
		UNICORN_DEVICE_SERIAL *availableDevices = new UNICORN_DEVICE_SERIAL[availableDevicesCount];
		errorCode = UNICORN_GetAvailableDevices(availableDevices, &availableDevicesCount, true);
		HandleError(errorCode);	

		// Print available device serials
		unsigned int deviceId = 0; // Select first device by default
		std::cout << "Available devices:" << std::endl;
		for (unsigned int i = 0; i < availableDevicesCount; i++) {
			if (strcmp(availableDevices[i], DEVICE_SERIAL) == 0) {
				deviceId = i;
			}
			std::cout << availableDevices[i] << std::endl;
		}

		// Open device
		std::cout << std::endl << "Trying to connect to '" << availableDevices[deviceId] << "'" << std::endl;
		errorCode = UNICORN_OpenDevice(availableDevices[deviceId], &deviceHandle);
		HandleError(errorCode);
		std::cout << "Connected to '" << availableDevices[deviceId] << "'" << std::endl;
		std::cout << "Device Handle: " << deviceHandle << std::endl;

		// Get information
		UNICORN_DEVICE_INFORMATION deviceInfo;
		errorCode = UNICORN_GetDeviceInformation(deviceHandle, &deviceInfo);
		HandleError(errorCode);
		std::cout << "EEG channels: " << deviceInfo.numberOfEegChannels << std::endl;
		std::cout << "Serial: " << deviceInfo.serial << std::endl;
		std::cout << "Firmware version: " << deviceInfo.firmwareVersion << std::endl;
		std::cout << "Device version: " << deviceInfo.deviceVersion << std::endl;
		std::cout << "PCB version: " << deviceInfo.pcbVersion << std::endl;
		std::cout << "Enclosure version: " << deviceInfo.enclosureVersion << std::endl;

		float* acquisitionBuffer = NULL;
		try {

			// Initialize acquisition members
			unsigned int numberOfChannelsToAcquire;
			UNICORN_GetNumberOfAcquiredChannels(deviceHandle, &numberOfChannelsToAcquire);

			// Get configuration
			UNICORN_AMPLIFIER_CONFIGURATION configuration;
			errorCode = UNICORN_GetConfiguration(deviceHandle, &configuration);
			HandleError(errorCode);

			// Print acquisition configuration
			std::cout << std::endl << "Acquisition Configuration:" << std::endl;
			std::cout << "Frame Length: " << FRAME_LENGTH << std::endl;
			std::cout << "Rate: " << UNICORN_SAMPLING_RATE << std::endl;
			std::cout << "Number Of Acquired Channels: " << numberOfChannelsToAcquire << std::endl;
			std::cout << std::left << std::setw (24) << "Name:" << "Unit:\tMin:\tMax:\tEnabled:" << std::endl;
			for (unsigned int i = 0; i < numberOfChannelsToAcquire; i++) {
				std::cout << std::left << std::setw (24) << configuration.Channels[i].name;
				std::cout << configuration.Channels[i].unit << "\t";
				std::cout << configuration.Channels[i].range[0] << "\t";
				std::cout << configuration.Channels[i].range[1] << "\t";
				std::cout << configuration.Channels[i].enabled << std::endl;
			}

			// Allocate memory for the acquisition buffer
			int acquisitionBufferLength = numberOfChannelsToAcquire * FRAME_LENGTH;
			acquisitionBuffer = new float[acquisitionBufferLength];

			// Create LSL outlet
			lsl::stream_info info(STREAM_NAME, STREAM_TYPE, numberOfChannelsToAcquire, UNICORN_SAMPLING_RATE, lsl::cf_float32, deviceInfo.serial);
			info.desc().append_child_value("manufacturer", "g.tec");
			lsl::xml_element chns = info.desc().append_child("channels");
			for (unsigned int i = 0; i < numberOfChannelsToAcquire; i++) {
				chns.append_child("channel")
				.append_child_value("label", configuration.Channels[i].name)
				.append_child_value("unit",configuration.Channels[i].unit);
			}
			lsl::stream_outlet outlet(info);
			std::cout << std::endl << "LSL stream with name '" << STREAM_NAME << "' and type '" << STREAM_TYPE << "' ready" << std::endl;

			// Start data acquisition
			errorCode = UNICORN_StartAcquisition(deviceHandle, TESTSIGNAL_ENABLED);
			HandleError(errorCode);
			std::cout << std::endl << "Data acquisition started" << std::endl;

			// Install signal handler
			// TODO: better handling of interrupt
			signal(SIGINT, OnClose);

			// Acquisition loop
			while (!stop) {

				// Receives the configured number of samples from the Unicorn device and writes it to the acquisition buffer
				errorCode = UNICORN_GetData(deviceHandle, FRAME_LENGTH, acquisitionBuffer, acquisitionBufferLength * sizeof(float));
				if (!stop) HandleError(errorCode);

				// Send sample to LSL outlet
				outlet.push_sample(acquisitionBuffer);

				// Print to screen
				/*
				for (unsigned int i = 0; i < numberOfChannelsToAcquire; i++) {
					std::cout << (float) acquisitionBuffer[i] << " ";
				}
				std::cout << std::endl;
				*/

			}

			// Stop data acquisition
			errorCode = UNICORN_StopAcquisition(deviceHandle);
			HandleError(errorCode);
			std::cout << std::endl << "Data acquisition stopped." << std::endl;

		} catch (int errorCode) {
			// Write error code to console if something goes wrong
			PrintErrorMessage(errorCode);
		} catch (...) {
			// Write error code to console if something goes wrong
			std::cout << std::endl << "An unknown error occurred" << std::endl;
		}

		// Free memory of the acquisition buffer if necessary.
		if (acquisitionBuffer != NULL) {
			delete[] acquisitionBuffer;
			acquisitionBuffer = NULL;
		}

		// Free memory of the device buffer if necessary.
		if (availableDevices != NULL) {
			delete[] availableDevices;
			availableDevices = NULL;
		}

		// Close device.
		errorCode = UNICORN_CloseDevice(&deviceHandle);
		HandleError(errorCode);
		std::cout << "Disconnected from Unicorn" << std::endl;

	} catch (int errorCode) {
		// Write error code to console if something goes wrong
		PrintErrorMessage(errorCode);
	} catch (...) {
		// Write error code to console if something goes wrong
		std::cout << std::endl << "An unknown error occurred" << std::endl;
	}
	return 0;
}

// The method throws an exception and forwards the error code if something goes wrong
void HandleError(int errorCode) {
	if (errorCode != UNICORN_ERROR_SUCCESS) {
		throw errorCode;
	}
}

// The method prints an error messag to the console according to the error code
void PrintErrorMessage(int errorCode) {
	std::cout << std::endl << "An error occurred. Error Code: " << errorCode << " - ";
	switch (errorCode) {
		case UNICORN_ERROR_INVALID_PARAMETER:
			std::cout << "One of the specified parameters does not contain a valid value.";
			break;
		case UNICORN_ERROR_BLUETOOTH_INIT_FAILED:
			std::cout << "The initialization of the Bluetooth adapter failed.";
			break;
		case UNICORN_ERROR_BLUETOOTH_SOCKET_FAILED:
			std::cout << "The operation could not be performed because the Bluetooth socket failed.";
			break;
		case UNICORN_ERROR_OPEN_DEVICE_FAILED:
			std::cout << "The device could not be opened.";
			break;
		case UNICORN_ERROR_INVALID_CONFIGURATION:
			std::cout << "The configuration is invalid.";
			break;
		case UNICORN_ERROR_BUFFER_OVERFLOW:
			std::cout << "The acquisition buffer is full.";
			break;
		case UNICORN_ERROR_BUFFER_UNDERFLOW:
			std::cout << "The acquisition buffer is empty.";
			break;
		case UNICORN_ERROR_OPERATION_NOT_ALLOWED:
			std::cout << "The operation is not allowed.";
			break;
		case UNICORN_ERROR_INVALID_HANDLE:
			std::cout << "The specified connection handle is invalid.";
			break;
		case UNICORN_ERROR_GENERAL_ERROR:
			std::cout << "An unspecified error occurred.";
			break;
		default:
			break;
	}
	std::cout << std::endl;
}

void OnClose(int signal) {
	stop = 1;
}

