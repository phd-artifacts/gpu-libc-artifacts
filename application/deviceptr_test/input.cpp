#include <omp.h>
#include <stdlib.h>

int main() {
  int isDevice = 0;
#pragma omp target map(from : isDevice)
  {
    isDevice = omp_is_initial_device();
  }
  return isDevice;
}