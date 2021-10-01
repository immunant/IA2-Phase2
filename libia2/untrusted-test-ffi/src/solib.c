#include <stdio.h>
#include <stdbool.h>

struct SharedValue {
    int x;
};

struct SecretValue {
    int x;
};

void untrusted_shared_read(struct SharedValue *shared) {
    printf("Shared@%p\n", shared);
    printf("Shared=%d\n", shared->x);
}
void untrusted_secret_read(struct SecretValue *secret) {
    printf("Secret@%p\n", secret);
    // Should fail in untrusted compartment
    printf("Secret=%d\n", secret->x);
}


void untrusted_foreign_function(
        struct SecretValue *secret,
        struct SharedValue *shared,
        bool deref_secret
    ) {
    printf("Shared@%p\n", shared);
    printf("Shared=%d\n", shared->x);

    printf("Secret@%p\n", secret);
    // Should fail in untrusted compartment
    if (deref_secret) printf("Secret=%d\n", secret->x);
}


void untrusted_foreign_function_with_callback(
        struct SecretValue *secret,
        struct SharedValue *shared,
        void (*cb)(struct SecretValue*),
        bool deref_secret
    ) {
    printf("Shared@%p\n", shared);
    printf("Shared=%d\n", shared->x);

    // Call the callback, should succeed
    (*cb)(secret);

    printf("Secret@%p\n", secret);
    // Should fail in untrusted compartment
    if (deref_secret) printf("Secret=%d\n", secret->x);
}

void untrusted_nop_function(int x) {
    return x;
}

void untrusted_nop_function2(int x) {
    return x;
}

void untrusted_nop_function3(int x) {
    return x;
}

void untrusted_nop_function4(int x) {
    return x;
}
