// PRMS Id: 8569

#include <iostream>
#include <vector>

using std::vector;
 
class Component {
    int george;
    char mabel[128];
};
class CopyMe {
public:
    CopyMe(){;}
private:
    vector<Component> strvec;
};

class IncludeIt   {
public:
    IncludeIt() {}
    ~IncludeIt() {}
    IncludeIt(const IncludeIt& i) {
        myStrvec = i.myStrvec;
    }
    IncludeIt& operator=(const IncludeIt& i) {
	myStrvec = i.myStrvec;
    }
private:
    CopyMe myStrvec;
};

int main(int argc, char**argv)   {
    IncludeIt foo;
    IncludeIt* bar;
    std::exit(0);
}

template class std::__malloc_alloc_template<0>;
#ifndef __USE_MALLOC
template class std::__default_alloc_template<false, 0>;
#endif





