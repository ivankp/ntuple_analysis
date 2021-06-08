# ntuple_analysis
A suite of programs and scripts for histogramming and reweighting of GoSam and BlackHat ntuples.

For the details of the structure of the ntuples, please refer to
[arXiv:1310.7439](https://arxiv.org/abs/1310.7439),
[arXiv:1608.01195](https://arxiv.org/abs/1608.01195), or
Appendix A of https://cds.cern.ch/record/2766569.

This readme is a work in progress.
I will continue adding to it to make it a comprehensive guide to working with GoSam ntuples.

# Quick start
## Check out the code
```
git clone --recursive git@github.com:ivankp/ntuple_analysis.git
```

Don't forget the `--recursive` argument to also clone the submodules.

## How to run
1. Edit `src/hist.cc`. This is the program that reads the ntuples and fills defined histograms.
It also does reweighting.
2. Add histograms' definitions
[around line 350](https://github.com/ivankp/ntuple_analysis/blob/master/src/hist.cc#L350).
Basically, add `h_(name_of_observable)`.
3. Fill histograms towards the end of the event loop,
[around line 510](https://github.com/ivankp/ntuple_analysis/blob/master/src/hist.cc#L510).
4. Define histograms' binning in `run/binning.json`.
5. Select what sets of ntuples to run over in `run/submit.sh`.
6. Generate job scripts and submit them to condor by running `./submit.sh` inside the `run` directory.
