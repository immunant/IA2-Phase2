# Threading in IA2

Supporting threads requires splitting initialization of global resources into those that are truly global and those that pertain to threads of control, of which there can now be multiple.

## Call stacks

In IA2, each compartment has its own call stack. Prior to threading support, these call stacks were initialized on program startup, and a pointer to the stack used by each compartment could be found at its index into the `ia2_stackptrs` global variable.[^ia2_stackptrs_shared]

### Stack pointers

In a threaded program, each thread has its own call stack; compartmentalizing a threaded program means that each thread now has a call stack for each compartment.
In addition, we want to make sure that finding the pointer to a given compartment's stack is still very efficient, as this must be done twice during each compartment transition
(to save the old compartment's stack pointer and load the new compartment's one).

**The chosen scheme is to replace `ia2_stackptrs` with a separate thread-local variable `ia2_stackptr_N` for each compartment, where N is the id of the compartment.**
This scheme does allow two threads that share a compartment to modify each other's stack pointer,
but this is not an increase in privilege because they can also modify all of each other's stack data by virtue of being in the same container.
Cross-compartment stack pointer modification is prevented by our existing TLS protection, because each `ia2_stackptr_N` is defined in the compartment whose stack pointers it holds, so it is `pkey_mprotect`ed along with other TLS variables from that compartment.

### Stack initialization

The stacks into which these stack pointers point must be allocated before we can switch into them. Because thread creation is already a nontrivial overhead, and we want to avoid introducing unnecessary branches into compartment transitions, **we chose to frontload the work of setting up stacks for all compartment when starting a thread**. Therefore, on thread creation, we must, for each compartment N:

1. allocate stack memory for the stack
2. protect that memory for the appropriate compartment
3. write the stack pointer into `ia2_stackptr_N`
	- this step requires being in that compartment, so we must save and restore the pkey here.

##### Footnotes

[^ia2_stackptrs_shared]: This scheme required `ia2_stackptrs` to be shared by all compartments, which meant that each compartment could possibly change the stack pointer of other compartments. This could, however, have been worked around by adding padding into `ia2_stackptrs` and `pkey_mprotect`ing each padded item to the appropriate compartment.
