# How to run
## Run test case 1 with model-checker
```Bash
> mkdir build
> cd build
> cmake -DMODEL_CHK_PATH=${model_checker_root} ..
# example:
> cmake -DMODEL_CHK_PATH=/home/mintyi/codework/model-checker ..
> make
> ./barrier -v -m 3
```
## Run test case 2 with pthread
```Bash
> cmake ..
> make
> ./barrier
```