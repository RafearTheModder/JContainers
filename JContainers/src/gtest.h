#pragma once

//#include "typedefs.h"
#include "meta.h"

namespace testing
{
    struct State;
    typedef void (*TestFn)(State&);
    struct TestInfo {
        TestFn function;
        const char* name;
        const char* name2;
    };

    inline TestInfo createInfo(TestFn function, const char* name, const char* name2) {
        TestInfo t = {function,name,name2};
        return t;
    }

    bool runTests(const meta<TestInfo>::list& list);
    void check(State& testState, bool, const char* source, const char* errorMessage);

    inline void check(State* testState, bool res, const char* source, const char* errorMessage) {
        check(*testState, res, source, errorMessage);
    }

    struct Fixture {
        State* testState;
        Fixture() : testState(nullptr) {}
    };

    template<class T>
    inline void FixtureTest(State& st) {
        T fixt;
        fixt.testState = &st;
        fixt.test();
    }

#   define TEST_F(FixtureBase, name, name2) \
        struct Fixture_##name##_##name2 : public FixtureBase { \
            void test(); \
        }; \
        static const meta<::testing::TestInfo> testInfo_##name##_##name2( \
            ::testing::createInfo(&::testing::FixtureTest<Fixture_##name##_##name2 >, #name, #name2)); \
        \
        void Fixture_##name##_##name2::test()

#   define TEST_F_DISABLED(FixtureBase, name, name2) TEST_F(FixtureBase, name, DISABLED_##name2)


#   define TEST(name, name2) \
        void TESTFUNC_NAME(name,name2)(::testing::State&); \
        static const meta<::testing::TestInfo> testInfo_##name##_##name2( \
            ::testing::createInfo(&TESTFUNC_NAME(name,name2),#name,#name2)); \
        void TESTFUNC_NAME(name,name2)(::testing::State& testState)

#   ifdef TEST_COMPILATION_DISABLED
#       define TEST2(name, name2, ...) TEST2_NOT_COMPILED(name, name2, __VA_ARGS__)
#   else
#       define TEST2(name, name2, ...) \
            void TESTFUNC_NAME(name,name2)(::testing::State& testState) { __VA_ARGS__;} \
            static const meta<::testing::TestInfo> testInfo_##name##_##name2( \
                ::testing::createInfo(&TESTFUNC_NAME(name,name2),#name,#name2));
#   endif

#   define TESTFUNC_NAME(name, name2) TestFunc_##name##_##name2

#   define TEST2_NOT_COMPILED(name, name2, ...) TEST(name, NOT_COMPILED_##name2) {}

#   define TEST_DISABLED(name, name2) TEST(name, DISABLED_##name2)

#   define EXPECT_TRUE(expression) ::testing::check(testState, expression, __FUNCTION__, #expression " is false");
#   define EXPECT_FALSE(expression)  ::testing::check(testState, !(expression), __FUNCTION__, #expression " is true");
#   define EXPECT_EQ(a, b) ::testing::check(testState, (a) == (b), __FUNCTION__, #a " != " #b);

#   define EXPECT_NOT_NIL(expression) ::testing::check(testState, (expression) != nullptr, __FUNCTION__, #expression " is null ");
#   define EXPECT_NIL(expression) ::testing::check(testState, (expression) == nullptr, __FUNCTION__, #expression " is not null ");

#   define EXPECT_THROW(expression, exception) \
        try { \
            expression; \
            ::testing::check(testState, false, __FUNCTION__, "'" #expression "' does not throws nor '" #exception "' nor any other exception"); \
        } \
        catch( const exception& ) {;} \
        catch(...) { \
            ::testing::check(testState, false, __FUNCTION__,  "'" #expression "' does not throws '" #exception "' exception, but throws unknown exception"); \
            throw; \
        }

#   define EXPECT_NOTHROW(expression) \
        try { \
            expression; \
        } catch(...) { \
            ::testing::check(testState, false, __FUNCTION__, "'" #expression "' throws exception"); \
        }
}
