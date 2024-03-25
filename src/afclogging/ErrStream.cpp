//

#include "ErrStream.h"

ErrStream::ErrStream()
{
}

ErrStream::operator QString() const
{
	return QString::fromStdString(_str.str());
}
