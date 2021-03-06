// RUN: $(dirname %s)/check_clang_tidy.sh %s misc-swapped-arguments %t
// REQUIRES: shell

void F(int, double);

int SomeFunction();

template <typename T, typename U>
void G(T a, U b) {
  F(a, b); // no-warning
  F(2.0, 4);
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: argument with implicit conversion from 'int' to 'double' followed by argument converted from 'double' to 'int', potentially swapped arguments.
// CHECK-FIXES: F(4, 2.0)
}

void foo() {
  F(1.0, 3);
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: argument with implicit conversion from 'int' to 'double' followed by argument converted from 'double' to 'int', potentially swapped arguments.
// CHECK-FIXES: F(3, 1.0)

#define M(x, y) x##y()

  double b = 1.0;
  F(b, M(Some, Function));
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: argument with implicit conversion from 'int' to 'double' followed by argument converted from 'double' to 'int', potentially swapped arguments.
// In macro, don't emit fixits.
// CHECK-FIXES: F(b, M(Some, Function));

#define N F(b, SomeFunction())

  N;
// CHECK-MESSAGES: :[[@LINE-1]]:3: warning: argument with implicit conversion from 'int' to 'double' followed by argument converted from 'double' to 'int', potentially swapped arguments.
// In macro, don't emit fixits.
// CHECK-FIXES: #define N F(b, SomeFunction())

  G(b, 3);
  G(3, 1.0);
  G(0, 0);

  F(1.0, 1.0);    // no-warning
  F(3, 1.0);      // no-warning
  F(true, false); // no-warning
  F(0, 'c');      // no-warning
}
