---
title: GDB调试初探
tags: [gdb]
categories: Linux 
---

久闻GDB大名，在 **The Exploit Laboratory** 一书中看到第一章就是介绍这个的，迅速跟进。


先上一段测试代码crash1.c:
    
{% codeblock crash1.c lang:c %}
	int main(int argc, char** argv)
	{
        int number;
        int *pointer;
        number = atoi((*argv)[1]);
        pointer = number;
        printnum(pointer);
	}

	void printnum(int* x)
	{
        printf("The number supplied is %d\n", *x);
	}
{% endcodeblock %}

<!-- more -->

编译 *gcc -o crash1 crash1.c* , 执行 *gdb crash1*，run运行程序发现报段错误，*backtrace* 查看调用信息，*x/10i $eip* 查看内存反汇编信息：

{% codeblock (gdb) x/10i $eip %}	
	   0xb7e55580:	movzbl (%edx),%eax
	   0xb7e55583:	mov    0x34(%esi),%ecx
	   0xb7e55586:	movsbl %al,%edx
	   0xb7e55589:	testb  $0x20,0x1(%ecx,%edx,2)
	   0xb7e5558e:	je     0xb7e555a1
	   0xb7e55590:	add    $0x1,%ebp
	   0xb7e55593:	movzbl 0x0(%ebp),%eax
	   0xb7e55597:	movsbl %al,%edx
	   0xb7e5559a:	testb  $0x20,0x1(%ecx,%edx,2)
	   0xb7e5559f:	jne    0xb7e55590
{% endcodeblock %}

这样一看基本上傻眼了，gdb默认反汇编为 *AT&T* 的语法风格，转成 *NASM* 风格会好懂一点，在用户目录下建立 *.gdbinit* 文件写入 *set disassembly-flavor intel* 让当前用户使用gdb时默认显示intel风格的汇编代码:

{% codeblock (gdb) x/10i $eip %}	
      0xb7e55580:	movzx  eax,BYTE PTR [edx]
	  0xb7e55583:	mov    ecx,DWORD PTR [esi+0x34]
	  0xb7e55586:	movsx  edx,al
	  0xb7e55589:	test   BYTE PTR [ecx+edx*2+0x1],0x20
	  0xb7e5558e:	je     0xb7e555a1
	  0xb7e55590:	add    ebp,0x1
	  0xb7e55593:	movzx  eax,BYTE PTR [ebp+0x0]
	  0xb7e55597:	movsx  edx,al
	  0xb7e5559a:	test   BYTE PTR [ecx+edx*2+0x1],0x20
	  0xb7e5559f:	jne    0xb7e55590
{% endcodeblock %}

现在就习惯了很多，最上面是最后一步的位置，将edx寄存器存放地址上取byte长度存入eax，查看寄存器信息:

{% codeblock (gdb) info registers %}	
        eax            0x0	0
		ecx            0xbffff774	-1073744012
		edx            0x68	104
		ebx            0xb7fc6ff4	-1208193036
		esp            0xbffff600	0xbffff600
		ebp            0x68	0x68
		esi            0xb7fc78c0	-1208190784
		edi            0x0	0
		eip            0xb7e55580	0xb7e55580
		eflags         0x10283	[ CF SF IF RF ]
		cs             0x73	115
		ss             0x7b	123
		ds             0x7b	123
		es             0x7b	123
		fs             0x0	0
		gs             0x33	51
{% endcodeblock %}

可以看到edx指向的地址是0x68，是一个不可执行地址，故触发异常。
