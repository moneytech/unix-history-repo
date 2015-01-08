//===-- tsan_mop.cc -------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
//===----------------------------------------------------------------------===//
#include "tsan_interface.h"
#include "tsan_test_util.h"
#include "gtest/gtest.h"
#include <stddef.h>
#include <stdint.h>

TEST(ThreadSanitizer, SimpleWrite) {
  ScopedThread t;
  MemLoc l;
  t.Write1(l);
}

TEST(ThreadSanitizer, SimpleWriteWrite) {
  ScopedThread t1, t2;
  MemLoc l1, l2;
  t1.Write1(l1);
  t2.Write1(l2);
}

TEST(ThreadSanitizer, WriteWriteRace) {
  ScopedThread t1, t2;
  MemLoc l;
  t1.Write1(l);
  t2.Write1(l, true);
}

TEST(ThreadSanitizer, ReadWriteRace) {
  ScopedThread t1, t2;
  MemLoc l;
  t1.Read1(l);
  t2.Write1(l, true);
}

TEST(ThreadSanitizer, WriteReadRace) {
  ScopedThread t1, t2;
  MemLoc l;
  t1.Write1(l);
  t2.Read1(l, true);
}

TEST(ThreadSanitizer, ReadReadNoRace) {
  ScopedThread t1, t2;
  MemLoc l;
  t1.Read1(l);
  t2.Read1(l);
}

TEST(ThreadSanitizer, WriteThenRead) {
  MemLoc l;
  ScopedThread t1, t2;
  t1.Write1(l);
  t1.Read1(l);
  t2.Read1(l, true);
}

TEST(ThreadSanitizer, WriteThenLockedRead) {
  Mutex m(Mutex::RW);
  MainThread t0;
  t0.Create(m);
  MemLoc l;
  {
    ScopedThread t1, t2;

    t1.Write8(l);

    t1.Lock(m);
    t1.Read8(l);
    t1.Unlock(m);

    t2.Read8(l, true);
  }
  t0.Destroy(m);
}

TEST(ThreadSanitizer, LockedWriteThenRead) {
  Mutex m(Mutex::RW);
  MainThread t0;
  t0.Create(m);
  MemLoc l;
  {
    ScopedThread t1, t2;

    t1.Lock(m);
    t1.Write8(l);
    t1.Unlock(m);

    t1.Read8(l);

    t2.Read8(l, true);
  }
  t0.Destroy(m);
}


TEST(ThreadSanitizer, RaceWithOffset) {
  ScopedThread t1, t2;
  {
    MemLoc l;
    t1.Access(l.loc(), true, 8, false);
    t2.Access((char*)l.loc() + 4, true, 4, true);
  }
  {
    MemLoc l;
    t1.Access(l.loc(), true, 8, false);
    t2.Access((char*)l.loc() + 7, true, 1, true);
  }
  {
    MemLoc l;
    t1.Access((char*)l.loc() + 4, true, 4, false);
    t2.Access((char*)l.loc() + 4, true, 2, true);
  }
  {
    MemLoc l;
    t1.Access((char*)l.loc() + 4, true, 4, false);
    t2.Access((char*)l.loc() + 6, true, 2, true);
  }
  {
    MemLoc l;
    t1.Access((char*)l.loc() + 3, true, 2, false);
    t2.Access((char*)l.loc() + 4, true, 1, true);
  }
  {
    MemLoc l;
    t1.Access((char*)l.loc() + 1, true, 8, false);
    t2.Access((char*)l.loc() + 3, true, 1, true);
  }
}

TEST(ThreadSanitizer, RaceWithOffset2) {
  ScopedThread t1, t2;
  {
    MemLoc l;
    t1.Access((char*)l.loc(), true, 4, false);
    t2.Access((char*)l.loc() + 2, true, 1, true);
  }
  {
    MemLoc l;
    t1.Access((char*)l.loc() + 2, true, 1, false);
    t2.Access((char*)l.loc(), true, 4, true);
  }
}

TEST(ThreadSanitizer, NoRaceWithOffset) {
  ScopedThread t1, t2;
  {
    MemLoc l;
    t1.Access(l.loc(), true, 4, false);
    t2.Access((char*)l.loc() + 4, true, 4, false);
  }
  {
    MemLoc l;
    t1.Access((char*)l.loc() + 3, true, 2, false);
    t2.Access((char*)l.loc() + 1, true, 2, false);
    t2.Access((char*)l.loc() + 5, true, 2, false);
  }
}

TEST(ThreadSanitizer, RaceWithDeadThread) {
  MemLoc l;
  ScopedThread t;
  ScopedThread().Write1(l);
  t.Write1(l, true);
}

TEST(ThreadSanitizer, BenignRaceOnVptr) {
  void *vptr_storage;
  MemLoc vptr(&vptr_storage), val;
  vptr_storage = val.loc();
  ScopedThread t1, t2;
  t1.VptrUpdate(vptr, val);
  t2.Read8(vptr);
}

TEST(ThreadSanitizer, HarmfulRaceOnVptr) {
  void *vptr_storage;
  MemLoc vptr(&vptr_storage), val1, val2;
  vptr_storage = val1.loc();
  ScopedThread t1, t2;
  t1.VptrUpdate(vptr, val2);
  t2.Read8(vptr, true);
}

static void foo() {
  volatile int x = 42;
  int x2 = x;
  (void)x2;
}

static void bar() {
  volatile int x = 43;
  int x2 = x;
  (void)x2;
}

TEST(ThreadSanitizer, ReportDeadThread) {
  MemLoc l;
  ScopedThread t1;
  {
    ScopedThread t2;
    t2.Call(&foo);
    t2.Call(&bar);
    t2.Write1(l);
  }
  t1.Write1(l, true);
}

struct ClassWithStatic {
  static int Data[4];
};

int ClassWithStatic::Data[4];

static void foobarbaz() {}

TEST(ThreadSanitizer, ReportRace) {
  ScopedThread t1;
  MainThread().Access(&ClassWithStatic::Data, true, 4, false);
  t1.Call(&foobarbaz);
  t1.Access(&ClassWithStatic::Data, true, 2, true);
  t1.Return();
}
