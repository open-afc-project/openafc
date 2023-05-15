#include "list.cpp"
#include "dbldbl.h"
#include "Vector3.h"

class ULSClass;
class RLANClass;
class ChannelPairClass;
class LidarBlacklistEntryClass;
class BBClass;

template class ListClass<char *>;
template class ListClass<int>;
template class ListClass<double>;
template class ListClass<DblDblClass>;
template class ListClass<ULSClass *>;
template class ListClass<ChannelPairClass *>;
template class ListClass<LidarBlacklistEntryClass *>;
template class ListClass<Vector3>;
template class ListClass<BBClass *>;

template void sort(ListClass<double> *);
template void sort(ListClass<DblDblClass> *);

template void sort2<double, int>(ListClass<double>*, ListClass<int>*);

