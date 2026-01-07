TODO:

Semantics is factored based on where the reference that is being written is located.
Implementation is based on remove and then add.

Semantics might be easier to optimise.  If we can special case based on the references location, 
then we can avoid some checks.

Knowing the targets previous location might help with optimisations.

write_from_frame
* write_from_frame vs write_barrier_frame?  Same?
* Shouldn't write_from_frame take the old region, it might need to do a stack dec?
* I think this case also needs a drag case if the frame-local region is longer lived than the target frame-local region.





Remove region reference  (ignoring classic RC operations for now)

|                    | Location of old Target                                                                                    |
| Location of Source | Stack | Frame Local | Region                                                     | Immutable              |
|--------------------|-------|-------------|------------------------------------------------------------|------------------------|
| Stack              |  Nop  | Nop         | Nop  (stack dec if dead)                                   |   Nop                  |
| Frame Local        |  NA   | Nop         | Nop  (stack dec if dead)                                   |   Nop                  |
| Region             |  NA   | NA          | Unparent + (stack inc if live) unless same region, then Nop|   Nop                  |


Add region reference  (ignoring classic RC operations for now)

|                    | Location of Target                                                                                              |
| Location of Source | Stack            | Frame Local                    | Region                        | Immutable                   |
|--------------------|------------------|--------------------------------|-------------------------------|-----------------------------|
| Stack              | Check Lifetime   | Check lifetime, drag if needed | Stack inc (if not move)       |  Nop                        |
| Frame Local        | Not allowed      | Check lifetime, drag if needed | Stack inc (if not move)       |  Nop                        |
| Region             | Not allowed      | Drag                           | Parent (stack dec if move)    |  Nop                        |




|  age  | STACK |

|  age  | FRAME_LOCAL |


flag =  STACK | FRAME_LOCAL

(~source_flag & target_flag)