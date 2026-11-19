//TestHCP65KernalExceptions' program, built with exceptions (CommonLibraries CppCompilerLib/ExceptionsPlan.txt), run
//by the kernal as a user process.  An exception thrown two calls down is caught by main, with an object of the
//caller's destroyed on the way.  A second one nobody catches leaves main: the start up (UserLib/UserStart.asm) sees it
//and calls std::terminate, whose handler ends the process with 77.  Any other exit says what went wrong.
#include <exception>
#include <stdlib.h>


struct SThrown
{
	int	iValue;
};


static int	giDestroyed;


struct CCounted
{
	~CCounted(void) { giDestroyed++; }
};


static int Thrower(int iValue)
{
	if (iValue > 0)
	{
		SThrown	sThrown;

		sThrown.iValue = iValue;
		throw sThrown;
	}
	return iValue;
}


static int Middle(int iValue)
{
	CCounted	cCounted;

	return Thrower(iValue) + 1;
}


static void Ended(void)
{
	_Exit(77);
}


int main(void)
{
	int	iCaught;

	iCaught = 0;
	try
	{
		Middle(5);
		return 1;
	}
	catch (const SThrown& sThrown)
	{
		iCaught = sThrown.iValue;
	}
	if ((iCaught != 5) || (giDestroyed != 1))
	{
		return 2;
	}
	if (Middle(-1) != 0)
	{
		return 3;
	}
	std::set_terminate(Ended);
	Middle(3);
	return 4;
}
