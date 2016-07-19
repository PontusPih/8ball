/*PDP-8 PAL ASSEMBLY LANGUAGE PROGRAM TO PRINT HELLO WORLD!*/

mem[00200] = 07200;//          CLA                     /CLEAR ACCUMULATOR
mem[00201] = 07100;//          CLL                     /CLEAR AC LINK
mem[00202] = 01220;//          TAD CHRSTR              /LOAD 1ST WRD OF CHRSTR (WHICH IS THE ADDR OF CHRSTR)
mem[00203] = 03010;//          DCA AIX1                /STORE THAT IN AUTOINDEX REG 1
mem[00204] = 07000;//  LOOP,   NOP                     /TOP OF LOOP TO READ AND PRINT STRING
//                                                     /I USE A NOP JUST TO MAKE IT EASIER TO
//                                                     /INSERT CODE BELOW THE LABEL.
mem[00205] = 01410;//          TAD I AIX1              /INCR ADDR IN AIX1, THEN LOAD AC FROM THAT
mem[00206] = 07450;//          SNA                     /IF AC IS NOT ZERO, SKIP NEXT INSTRUCTION
mem[00207] = OPR | OPR_G2 | HLT; //05577;//          JMP I [7600             /EXIT PROGRAM (BACK TO MONITOR)
mem[00210] = 04212;//          JMS TTYO                /CALL OUTPUT ROUTINE
mem[00211] = 05204;//          JMP LOOP                /REPEAT LOOP
mem[00212] = 00000;//  TTYO,   0                       /TTY OUTPUT ROUTINE. THE FIRST WORD OF
//                                                     /A SUBROUTINE MUST BE EMPTY (0) BECAUSE
//                                                     /THE JMS INSTRUCTION INSERTS THE RETURN
//                                                     /ADDR IN THIS WORD.
mem[00213] = 06046;//          TLS                     /WRITE AC TO THE OUTPUT DEVICE (TTY)
mem[00214] = 06041;//          TSF                     /IF TTY IS READY, SKIP NEXT INSTRUCTION.
mem[00215] = 05214;//          JMP .-1                 /TTY IS NOT READY, SO CHECK AGAIN
mem[00216] = 07200;//          CLA                     /CLEAR AC
mem[00217] = 05612;//          JMP I TTYO              /RETURN TO CALLER
             
mem[00220] = 00220;//  CHRSTR, .                       /1ST WORD IS ADDR OF STRING
mem[00221] = 00110;//          110                     /H
mem[00222] = 00105;//          105                     /E
mem[00223] = 00114;//          114                     /L
mem[00224] = 00114;//          114                     /L
mem[00225] = 00117;//          117                     /O
mem[00226] = 00040;//          040                     /
mem[00227] = 00127;//          127                     /W
mem[00230] = 00117;//          117                     /O
mem[00231] = 00122;//          122                     /R
mem[00232] = 00114;//          114                     /L
mem[00233] = 00104;//          104                     /D
mem[00234] = 00041;//          041                     /!
mem[00235] = '\n';//          000                     /<EOT>
mem[00177] = 07600;//

