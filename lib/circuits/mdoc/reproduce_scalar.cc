#include "../../ec/p256.h"
#include <iostream>

using namespace proofs;

using Scalar = typename proofs::Fp256Scalar::Elt;

int main() {
  Scalar s;
  std::cout << "Scalar type exists." << std::endl;
  return 0;
}
