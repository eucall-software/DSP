#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <time.h>
#include <iostream>

class StopWatch
{
private:
    int state;
    timespec _start;
    timespec _end;
public:

	StopWatch();
	int start();
	int stop();
	double elapsedSeconds() const;
	friend std::ostream& operator<<(std::ostream& lhs, const StopWatch& stopWatch);
};

#endif


