/* AUTOGENERATED FILE. DO NOT EDIT. */

//=======Test Runner Used To Run Each Test Below=====
#define RUN_TEST(TestFunc, TestLineNum) \
{ \
  Unity.CurrentTestName = #TestFunc; \
  Unity.CurrentTestLineNumber = TestLineNum; \
  Unity.NumberOfTests++; \
  if (TEST_PROTECT()) \
  { \
      setUp(); \
      TestFunc(); \
  } \
  if (TEST_PROTECT() && !TEST_IS_IGNORED) \
  { \
    tearDown(); \
  } \
  UnityConcludeTest(); \
}

//=======Automagically Detected Files To Include=====
#include "unity.h"
#include <setjmp.h>
#include <stdio.h>

//=======External Functions This Runner Calls=====
extern void setUp(void);
extern void tearDown(void);
extern void test_ReadEmptyFile();
extern void test_ReadCorrectFile();
extern void test_ReadFileWithBlankLines();
extern void test_WriteEmptyFile();
extern void test_WriteFileWithSingleEntry();
extern void test_WriteFileWithMultipleEntries();


//=======Test Reset Option=====
void resetTest()
{
  tearDown();
  setUp();
}

char *progname;


//=======MAIN=====
int main(int argc, char *argv[])
{
  progname = argv[0];
  Unity.TestFile = "kodFile.c";
  UnityBegin("kodFile.c");
  RUN_TEST(test_ReadEmptyFile, 29);
  RUN_TEST(test_ReadCorrectFile, 35);
  RUN_TEST(test_ReadFileWithBlankLines, 53);
  RUN_TEST(test_WriteEmptyFile, 76);
  RUN_TEST(test_WriteFileWithSingleEntry, 92);
  RUN_TEST(test_WriteFileWithMultipleEntries, 116);

  return (UnityEnd());
}
