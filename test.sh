#!/bin/bash

make
./8ball --restore tests/maindec-8e-d0ab-pb.core --exit_on_HLT --run --stop_at 05314
