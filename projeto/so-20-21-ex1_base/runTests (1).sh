#! /bin/sh

#variaveis lidas
$ read -p runTests inputdir outputdir maxthreads

#se maxthreads <= 0 retorna erro
if [$maxthreads <= 0]; then
  echo "Erro, numero de threads invalido"
  exit 1
fi

#se inputdir nao existir
if [ -d "$inputdir"]; then
else
   echo "Erro, diretoria invalida"
   exit 1 
fi

#se outputdir nao existir
if [ -d "$outputdir"]; then
else
   echo "Erro, diretoria invalida"
   exit 1 
fi

FILES=inputdir 
#ciclo com cada input na diretoria 
for input in $FILES
do
  #ciclo que corre o programa de uma thread ate ao maximo de threads
  for (( threads = 1; threads <= maxthreads; threads++ )) 
        do 
        echo "InputFile=$input NumThreads=$threads"
        ./tecnicofs $input outputdir/$input-$threads.txt $threads  
	done
done 