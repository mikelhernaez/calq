/** @file Exceptions.h
 *  @brief This files contains the definitions of some custom exception 
 *         classes.
 *  @author Jan Voges (voges)
 *  @bug No known bugs
 */

/*
 *  Changelog
 *  YYYY-MM-DD: What (who)
 */

#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "Common/helpers.h"
#include <exception>
#include <iostream>
#include <string>

/** @brief Class: Exception
 *
 *  Specific exception classes inherit from this class.
 */
class Exception : public std::exception {
public:
    explicit Exception(const std::string &msg);
    virtual ~Exception(void) throw();
    virtual std::string getMessage(void) const;
    virtual const char * what(void) const throw();

protected:
    std::string msg;
};

/** @brief Class: ErrorException
 *
 *  This exception is thrown by calls to the throwErrorException function.
 *  If an ErrorException is thrown at any point in the range of the try block
 *  (including in functions called from that code), control will jump
 *  immediately to the error handler.
 */
class ErrorException : public Exception {
public:
    explicit ErrorException(const std::string &msg): Exception(msg) {}
};

/** @brief Global function: throwErrorException
 *
 *  Signals an error condition in a program by throwing an ErrorException
 *  with the specified message.
 *
 *  @param msg Error message
 *  @return Void
 */
inline void throwErrorException(const std::string &msg)
{
    std::cout.flush();
    throw ErrorException(msg);
}

/** @brief Class: ErrorExceptionReporter
 *
 *  Reporter class catching the caller of an ErrorException.
 */
class ErrorExceptionReporter {
public:
    ErrorExceptionReporter(const std::string &file, 
                           const std::string &function,
                           const int line)
        : file(file)
        , function(function)
        , line(line)
    {}

    void operator()(const std::string &msg)
    {
        //std::cerr << file << ":" << function << ":" << line << ": ";
        std::string tmp = fileBaseName(file) + ":" + function + ":" + std::to_string(line) + ": " + msg;
        // Can use the original name here, as it is still defined
        throwErrorException(tmp);
    }

private:
    std::string file;
    std::string function;
    int line;
};

// Remove the symbol for the function, then define a new version that instead
// creates a stack temporary instance of ErrorExceptionReporter initialized
// with the caller.
#undef throwErrorException
#define throwErrorException ErrorExceptionReporter(__FILE__, __FUNCTION__, __LINE__)

#endif // EXCEPTIONS_H

