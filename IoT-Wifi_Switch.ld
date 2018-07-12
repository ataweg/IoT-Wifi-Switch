OUTPUT_FORMAT("elf32-xtensa-le")


SECTIONS
{

  .irom0.text : ALIGN(4)
  {

/* AWe 2017-09-02 added more libs to this segment
 *  excluding, otherwise program crashes
 *  *libmain.a:(.literal.* .text.*)
 *  *libdriver.a:(.literal.* .text.*)
 */
    *libairkiss.a:(.literal.* .text.*)
    *libc.a:(.literal.* .text.*)
    *libgcc.a:(.literal.* .text.*)
    *liblwip_536.a:(.literal.* .text.*)
    *libmesh.a:(.literal.* .text.*)
    *libphy.a:(.literal.* .text.*)
    *libpp.a:(.literal.* .text.*)
    *libpwm.a:(.literal.* .text.*)
   } >irom0_0_seg :irom0_0_phdr
}