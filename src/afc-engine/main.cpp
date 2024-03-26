#include <chrono>

#include "AfcManager.h"
#include "afclogging/QtStream.h"
#include "afclogging/LoggingConfig.h"

namespace
{
// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "main")

int showErrorMessage(const std::string &message)
{
	LOGGER_CRIT(logger) << "AFC Engine error: " << message;
	// logging messages are all sent to stdout so when we want to display this
	// error to the user we manually use stderr
	std::cerr << "AFC Engine error: " << message << std::endl;
	Logging::flush();

	return 1;
}
} // end namespace

int main(int argc, char **argv)
{ // Accepts input from command line
	try {
		// initialize logging
		QtStream::installLogHandler();
		Logging::Config conf = Logging::Config();
		Logging::Filter filter = Logging::Filter();
		filter.setLevel("debug");
		conf.useStdOut = true;
		conf.useStdErr = false;
		conf.filter = filter;
		Logging::initialize(conf);

		std::string inputFilePath, configFilePath, outputFilePath, tempDir, logLevel;
		AfcManager afcManager = AfcManager();
		// Parse command line parameters
		try {
			afcManager.setCmdLineParams(inputFilePath,
						    configFilePath,
						    outputFilePath,
						    tempDir,
						    logLevel,
						    argc,
						    argv);
			conf.filter.setLevel(logLevel);
			Logging::initialize(conf); // reinitialize log level
		} catch (std::exception &err) {
			throw std::runtime_error(ErrStream() << "Failed to parse command line "
								"arguments provided by GUI: "
							     << err.what());
		}

		/**************************************************************************************/
		/* Read in the input configuration and parameters */
		/**************************************************************************************/

		// Set constant parameters
		afcManager.setConstInputs(tempDir);

		// Import configuration from the GUI
		LOGGER_DEBUG(logger) << "AFC Engine is importing configuration...";
		try {
			afcManager.importConfigAFCjson(configFilePath, tempDir);
		} catch (std::exception &err) {
			throw std::runtime_error(ErrStream()
						 << "Failed to import configuration from GUI: "
						 << err.what());
		}

		// Import user inputs from the GUI
		LOGGER_DEBUG(logger) << "AFC Engine is importing user inputs...";
		try {
			afcManager.importGUIjson(
				inputFilePath); // Reads the JSON file provided by the GUI
		} catch (std::exception &err) {
			throw std::runtime_error(ErrStream()
						 << "Failed to import user inputs from GUI: "
						 << err.what());
		}

		/**************************************************************************************/

		// Prints user input files for debugging
		afcManager.printUserInputs();

		// Read in the databases' information
		try {
			LOGGER_DEBUG(logger) << "initializing databases";
			auto t1 = std::chrono::high_resolution_clock::now();
			afcManager.initializeDatabases();
			auto t2 = std::chrono::high_resolution_clock::now();
			LOGGER_INFO(logger)
				<< "Databases initialized in: "
				<< std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count()
				<< " seconds";
		} catch (std::exception &err) {
			throw std::runtime_error(ErrStream() << "Failed to initialize databases: "
							     << err.what());
		}
		/**************************************************************************************/
		/* Perform AFC Engine Computations */
		/**************************************************************************************/
		auto t1 = std::chrono::high_resolution_clock::now();
		afcManager.compute();
		auto t2 = std::chrono::high_resolution_clock::now();
		LOGGER_INFO(logger)
			<< "Computations completed in: "
			<< std::chrono::duration_cast<std::chrono::seconds>(t2 - t1).count()
			<< " seconds";
		/**************************************************************************************/

#if 0
		std::vector<psdFreqRangeClass> psdFreqRangeList;
		afcManager.computeInquiredFreqRangesPSD(psdFreqRangeList);
#endif

		/**************************************************************************************/
		/* Write output files */
		/**************************************************************************************/
		QString outputPath = QString::fromStdString(outputFilePath);
		afcManager.exportGUIjson(outputPath, tempDir);

		LOGGER_DEBUG(logger) << "AFC Engine has exported the data for the GUI...";
		/**************************************************************************************/

		return 0;
	} catch (std::exception &e) {
		return showErrorMessage(e.what());
	}
}
