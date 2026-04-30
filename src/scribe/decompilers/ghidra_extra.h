#define SCARRY8(a, b) \
    ((uint8_t)(a) + (uint8_t)(b) > 0xFF)

#define SEXT816(a) \
    ((int16_t)(int8_t)(a))

/*
 * __readfsqword / __readgsqword
*/
#if defined(__x86_64__)
inline __attribute__((always_inline)) uint64_t __readfsqword(uint32_t a1) {
  uint64_t result; // rax
  asm volatile ("movq %%fs:%1, %0" : "=r" (result) : "m" (*((uint64_t*)a1)));
  return result;
}

inline __attribute__((always_inline)) uint64_t __readgsqword(uint32_t a1) {
  uint64_t result; // rax
  asm volatile ("movq %%gs:%1, %0" : "=r" (result) : "m" (*((uint64_t*)a1)));
  return result;
}
#endif
