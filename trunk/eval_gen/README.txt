For details please read the comments in eval_gen.cpp
To build a evaluation table, follow these steps:

1. Run eval_gen, it will create a eval.tbl file

2. Run eval_learn, it will refine the generated eval.tbl.

3. Run eval_learn several time will make more improvement, so iterate it 3 ~ 5 times if you are patient. Another subroutine eval_looper will start a infinite loop of eval_learn automatically. If you want to stop it, just press <Ctrl + C>. 
(It is not a infinite improvement, 5 times are enough.)

4. Execute command line:
	minigzip_d -9 eval.tbl
It will be compressed to a eval.tbl.gz file. Rename as "eval.ks" and copy it to kalscope path.