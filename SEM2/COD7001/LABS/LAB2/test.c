#include <stdio.h>
#include <unistd.h>

void target_function() {
    int a = 10;
    int b = 20;
    int sum = a + b;
    printf("Sum is: %d\n", sum);
}

int main() {
    printf("Starting program.\n");
    target_function();
    printf("Program finished.\n");
    return 0;
}