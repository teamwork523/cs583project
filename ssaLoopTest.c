#include "stdio.h"

int main() {
    int i = 0;
    int a[] = {1,2,3,4,5};
    int b[] = {6,7,8,9,10};
    for (i = 0; i < 100; i++) {
        // b[1] = a[0] + 1;
        if (i < 10) { 
            a[1] = a[3] + a[2];
            b[0] = a[1];
        } else {
            a[2] = 5;
            a[4] = a[2];
            a[2] = 8;
        }
        a[1] = 5;
        // b[0] = 8;
    }
    for (i = 0; i < 5; i++) {
        printf("%d ", a[i]);
    }
    printf("\n");
    for (i = 0; i < 5; i++) {
        printf("%d ", b[i]);
    }
    printf("\n");
    return 0;
}
