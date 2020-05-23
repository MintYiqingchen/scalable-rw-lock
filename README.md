# How to run
## Run test case with model-checker
```Bash
> mkdir build
> cd build
> cmake -DMODEL_CHK_PATH=${model_checker_root} ..
# example:
> cmake -DMODEL_CHK_PATH=/home/mintyi/codework/model-checker ..
> make
> ./simpleRwlock -v -m 1 -x 100 2 50
> ./snziRwlock -v -m 1 -x 100 2 50
```
## Run test case 2 with pthread
```Bash
> cmake ..
> make
> ./barrier
```