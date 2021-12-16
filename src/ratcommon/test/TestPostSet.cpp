// 

#include "../PostSet.h"
#include "rkfunittest/GtestShim.h"

namespace{
class BadCls{
 public:
    int val = 0;

    BadCls(int init=0) : val(init){}

    BadCls & operator = (const BadCls &other){
        if (other.val == 0) {
            throw std::logic_error("never");
        }
        val = other.val;
        return *this;
    }
};
}

TEST(TestPostSet, makeOneArg){
    int var = 0;
    {
        auto ps = make_PostSet(var, 2);
        ASSERT_EQ(0, var);
    }
    ASSERT_EQ(2, var);
}

TEST(TestPostSet, makeTwoArg){
    int var = 0;
    {
        auto ps = make_PostSet(var, 1, 2);
        ASSERT_EQ(1, var);
    }
    ASSERT_EQ(2, var);
}

TEST(TestPostSet, testPreException){
    BadCls var(-1);
    ASSERT_EQ(-1, var.val);

    ASSERT_THROW(
        make_PostSet(var, BadCls(0), BadCls(1)),
        std::logic_error
    );
    ASSERT_EQ(-1, var.val);
}

TEST(TestPostSet, testPostException){
    BadCls var(-1);
    ASSERT_EQ(-1, var.val);
    {
        auto ps = make_PostSet(var, BadCls(1), BadCls(0));
        ASSERT_EQ(1, var.val);
    }
    ASSERT_EQ(1, var.val);
}
