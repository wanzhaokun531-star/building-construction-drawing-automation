// No CAD. Minimal native modeless dialog for actual cross-desktop ownership tests.
using System;
using System.Runtime.InteropServices;
using System.Text;
internal static class IsolatedDesktopGuiTestHost {
 delegate IntPtr DialogProc(IntPtr window,uint message,UIntPtr w,IntPtr l);
 static readonly DialogProc Callback=HandleDialog;
 [StructLayout(LayoutKind.Sequential)] struct Point { public int X,Y; }
 [StructLayout(LayoutKind.Sequential)] struct Message {public IntPtr Window;public uint Id;public UIntPtr W;public IntPtr L;public uint Time;public Point Point;public uint Private;}
 [DllImport("user32.dll",SetLastError=true)] static extern IntPtr CreateDialogIndirectParamW(IntPtr module,IntPtr template,IntPtr parent,DialogProc callback,IntPtr parameter);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool SetWindowTextW(IntPtr window,string text);
 [DllImport("user32.dll")] static extern bool DestroyWindow(IntPtr window);
 [DllImport("user32.dll")] static extern void PostQuitMessage(int code);
 [DllImport("user32.dll")] static extern UIntPtr SetTimer(IntPtr window,UIntPtr id,uint delay,IntPtr callback);
 [DllImport("user32.dll")] static extern int GetMessageW(out Message message,IntPtr window,uint min,uint max);
 [DllImport("user32.dll")] static extern bool TranslateMessage(ref Message message);
 [DllImport("user32.dll")] static extern IntPtr DispatchMessageW(ref Message message);
 static IntPtr HandleDialog(IntPtr window,uint message,UIntPtr w,IntPtr l){
  if(message==0x0110){SetTimer(window,new UIntPtr(1),15000,IntPtr.Zero);return new IntPtr(1);}
  if(message==0x0010||message==0x0113){DestroyWindow(window);PostQuitMessage(message==0x0010?0:75);return new IntPtr(1);}return IntPtr.Zero;
 }
 [STAThread] static int Main(string[] args){
  // DLGTEMPLATE + empty menu/default #32770 class/empty title, word-aligned.
  byte[] data=new byte[24];Array.Copy(BitConverter.GetBytes(0x90C80000u),0,data,0,4);
  Array.Copy(BitConverter.GetBytes((short)10),0,data,10,2);Array.Copy(BitConverter.GetBytes((short)10),0,data,12,2);
  Array.Copy(BitConverter.GetBytes((short)160),0,data,14,2);Array.Copy(BitConverter.GetBytes((short)70),0,data,16,2);
  IntPtr buffer=Marshal.AllocHGlobal(data.Length);
  try{Marshal.Copy(data,0,buffer,data.Length);IntPtr window=CreateDialogIndirectParamW(IntPtr.Zero,buffer,IntPtr.Zero,Callback,IntPtr.Zero);if(window==IntPtr.Zero)return 76;
   SetWindowTextW(window,args.Length>0?args[0]:"Codex isolation machine helper");Message msg;int result;while((result=GetMessageW(out msg,IntPtr.Zero,0,0))>0){TranslateMessage(ref msg);DispatchMessageW(ref msg);}GC.KeepAlive(Callback);return result<0?77:unchecked((int)msg.W.ToUInt64());
  }finally{Marshal.FreeHGlobal(buffer);}
 }
}
