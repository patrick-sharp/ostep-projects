# Steps
1. run `make test`

OR:

1. run `make` to create `xcheck` and `create-tests`.
2. run `./create-tests`. This populates the tests/\* files (except for tests 1 and 2, which are manually created).
3. run `./test-xcheck.sh` to test `xcheck`.


# Other notes
`base-mkfs.img` is a file generated with the `mkfs` tool from xv6.

`print-base-img-diff.sh` is a script I used for verifying that `create-tests` 
produces an image file that is identical to `base-mkfs.img` for the base case.
