Viewport Evaluation
===================

Question
--------

is `WorldMap::Viewport` worth the effort, or is `WorldMap::DumbViewport`
good enough?


Experiment
----------

Try to find a path, measure time. Try dumb and normal viewport with and
without compiler optimisation.

Tested with `./launch.sh --bot-offline` with `output1.txt`


Results
-------

based on 661e1e38d24f

- Dumb + no optimisation = no patch:		17715ms
- Normal + no opt	= `norm_noopt.patch`:	10220ms
- Dynamic + no opt	= `dyn_noopt.patch`:	 9932ms

- Dumb + `-O2`		= `dumb_o2.patch`:	 1652ms
- Normal + `-O2`	= `norm_O2.patch`:	  491ms
- Dynamic + `-O2` 	= `dyn_O2.patch`:	  489ms


Conclusion
----------

`Viewport` is worth the effort, and growing the viewport dynamically,
on-demand is equally fast `:)`
