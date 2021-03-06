#!/bin/bash
test=TestKmerMatchMPI
MPI=""
MPI_OPTS=""
procs=$(($(lscpu -p | tail -1 | awk -F, '{print $2}')+1))

true=$(which true)

ismpi=0
if mpirun $true
then
  MPI="mpirun"
  if $MPI -bycore -bind-to-core $true
  then
    MPI_OPTS="-bycore -bind-to-core -np"
  else
    MPI_OPTS="-np"
  fi
  ismpi=1
elif aprun -n 1 $true
then
  MPI="aprun"
  MPI_OPTS="-n"
  procs=$(aprun -B -q uname -n | wc -l)
fi

set -e
set -x
if [ -n "$MPI" ]
then
  for mpi in 1 2 3 4 6 7 8 12 13 16 20 24 32
  do
    if [ $mpi -gt $procs ]
    then
      break
    fi
    export OMP_NUM_THREADS=1
    $MPI $MPI_OPTS $mpi $test
  done
  
  if ((ismpi)) && $MPI -bysocket -bind-to-socket $true
  then
   MPI_OPTS="-bysocket -bind-to-socket -np"
  fi
   
  echo "Running in hybrid mode"
 
  for threads in $(seq 1 $procs)
  do
    if ((ismpi)) && ((threads<=procs/2))
    then
      socket=''
    fi

    mpi=$(((procs+threads-1)/threads))
    
    if ((ismpi==0))
    then
      socket="-d $threads"     
    fi

    export OMP_NUM_THREADS=$threads
    $MPI $MPI_OPTS $mpi $socket $test
  done
fi


