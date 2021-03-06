# Relatório Exercício 4                                                                  

##### Pedro Rodrigues Grijó
##### 139715
<br/> 

## Introdução
Os objetivos deste exercício foram a integração de um processador, memória e periféricos através de um roteador; Implementação de um periférico simples e implementar e fazer uso de uma plataforma multicore.

## Periférico Básico

Inicialmente foi criada a aplicação [hello_world.c](https://github.com/pedrogrijo/MC723/blob/master/exercicio4/hello_world.c) para observar a execução do simulador. A aplicação faz escrita no endereço base(1024\*1024\*100 = 0x6400000) do periférico. Abaixo está a saída do simulador:

![](/exercicio4/images/sim_output1.png)

Apesar de o valor esperado ser 1, o valor 0 é impresso na saída

Após isso, foram modificadas as funções readm e writem do programa [ac_tlm_peripheral.cpp](https://github.com/pedrogrijo/MC723/blob/master/exercicio4/ac_tlm_peripheral.cppc) (aqui já estão as modificações do passo a seguir) e adicionada uma variável global para armazenar e manipular os valores lidos/escritos, e a aplicação [hello_modified.c](https://github.com/pedrogrijo/MC723/blob/master/exercicio4/hello_modified.c) foi criada. Como pode ser visto na imagem abaixo, após as modificações a saída ocorre conforme o esperado.

![](/exercicio4/images/sim_output2.png)

O ponteiro de acesso ao periférico dessa aplicação foi declarado como **volatile**. Isso é feito para indicar que um valor pode mudar entre acessos diferentes e previne que um compilador otimize as próximas leituras ou escritas e acabe usando valores incorretos que já não são os atuais.[1]

O último passo foi alterar a funcionalidade do periférico e a criação de uma aplicação que faz várias leituras no mesmo. Os resultados estão em [ac_tlm_peripheral.cpp](https://github.com/pedrogrijo/MC723/blob/master/exercicio4/ac_tlm_peripheral.cppc) e [hello_modified2.c](https://github.com/pedrogrijo/MC723/blob/master/exercicio4/hello_modified2.c) e a saída é: 

![](/exercicio4/images/sim_output3.png)

## Plataforma Multicore

## Referências

1. https://en.wikipedia.org/wiki/Volatile_(computer_programming)
