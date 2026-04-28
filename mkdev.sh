cd testers
touch fibEight.uriscv 
touch shell.uriscv 
touch fibEleven.uriscv 
touch echo.uriscv 
touch uname.uriscv 
touch sl.uriscv 
touch date.uriscv 
uriscv-mkdev -f calc.uriscv calc
uriscv-mkdev -f fibEight.uriscv fibEight
uriscv-mkdev -f shell.uriscv shell
uriscv-mkdev -f fibEleven.uriscv fibEleven
uriscv-mkdev -f echo.uriscv echo
uriscv-mkdev -f uname.uriscv uname
uriscv-mkdev -f sl.uriscv sl
uriscv-mkdev -f date.uriscv date
