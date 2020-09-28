using System;
using System.IO.Ports;
using System.Globalization;
using System.Threading;
using System.Runtime;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Linq;

namespace Crosscorrelator
{

	class libxc {
		[DllImport("ahp_xc")]
		public extern static int xc_get_baudrate();
		[DllImport("ahp_xc")]
		public extern static int xc_get_bps();
		[DllImport("ahp_xc")]
		public extern static int xc_get_nlines();
		[DllImport("ahp_xc")]
		public extern static int xc_get_nbaselines();
		[DllImport("ahp_xc")]
		public extern static int xc_get_delaysize();
		[DllImport("ahp_xc")]
		public extern static int xc_get_frequency();
		[DllImport("ahp_xc")]
		public extern static int xc_get_packetsize();
		[DllImport("ahp_xc")]
		public extern static uint xc_get_packettime();
		[DllImport("ahp_xc")]
		public extern static int xc_connect(string port);
		[DllImport("ahp_xc")]
		public extern static int xc_get_properties();
		[DllImport("ahp_xc")]
		public extern static void xc_disconnect();
		[DllImport("ahp_xc")]
		public extern static void xc_enable_capture(int enable);
		[DllImport("ahp_xc")]
		public extern static void xc_select_input(int index);
		[DllImport("ahp_xc")]
		public extern static void xc_set_baudrate(baud_rate rate, int setterm);
		[DllImport("ahp_xc")]
		public extern static void xc_set_power(int index, bool lv, bool hv);
		[DllImport("ahp_xc")]
		public extern static void xc_set_delay(int index, byte value);
		[DllImport("ahp_xc")]
		public extern static void xc_set_line(int index, byte value);
		[DllImport("ahp_xc")]
		public extern static void xc_set_frequency_divider(byte value);
		[DllImport("ahp_xc")]
		public extern static void xc_scan_autocorrelations(ulong[] spectrum, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static void xc_scan_crosscorrelations(ulong[] crosscorrelations, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static void xc_get_packet(ulong[] counts, ulong[] autocorrelations, ulong[] correlations);
		[DllImport("ahp_xc")]
		public extern static int xc_send_command(it_cmd c, byte value);
	};

	public enum baud_rate : int {
		R_57600 = 0,
		R_115200 = 1,
		R_230400 = 2,
		R_460800 = 3,
	};

	public enum it_cmd {
		CLEAR = 0,
		SET_INDEX = 1,
		SET_LEDS = 2,
		SET_BAUD_RATE = 3,
		SET_DELAY_LO = 4,
		SET_DELAY_HI = 5,
		SET_LINE_LO = 6,
		SET_LINE_HI = 7,
		SET_FREQ_DIV = 8,
		ENABLE_CAPTURE = 13
	};

	public enum OperatingMode {
		Counter = 0,
		Autocorrelator = 1,
		Crosscorrelator = 2,
	};

	public class PacketReceivedEventArgs : EventArgs
	{
		public ulong[] Counts;
		public ulong[] Lines;
		public ulong[] Correlations;
		public PacketReceivedEventArgs(ulong [] counts, ulong [] lines,ulong [] correlations)
		{
			Counts = counts;
			Lines = lines;
			Correlations = correlations;
		}
	}

	public class SweepUpdateEventArgs : EventArgs
	{
		public List<ulong> Counts;
		public int Index;
		public OperatingMode Mode;
		public SweepUpdateEventArgs(int index, List<ulong> counts, OperatingMode mode)
		{
			Mode = mode;
			Index = index;
			Counts = counts;
		}
	}

	public class ConnectionEventArgs : EventArgs
	{
		public bool Connected;
		public ConnectionEventArgs(bool connected)
		{
			Connected = connected;
		}
	}

	public class XC
	{
		public int PacketSize  { get { return libxc.xc_get_packetsize(); } }
		public double PacketTime  { get { return (double)libxc.xc_get_packettime() / 1000000.0; } }
		public int NumLines { get { return libxc.xc_get_nlines(); } }
		public int NumBaselines { get { return libxc.xc_get_nbaselines(); } }
		public int ClockFrequency { get { return libxc.xc_get_frequency(); } }
		public int BitsPerSample { get { return libxc.xc_get_bps(); } }
		public int DelaySize { get { return libxc.xc_get_delaysize(); } }
		public int BaudRate { get { return libxc.xc_get_baudrate(); } }

		public List<ulong>[] Counts;
		ulong[] Autocorrelations;
		ulong[] Crosscorrelations;

		Thread _readThread;
		Timer _timer;
		bool ThreadsRunning;
		bool _connected;
		bool Reading { get { return (_Reading == 0); } set { _Reading = (value ? 0 : 1); } } 
		int _Reading = 1;
		public double Percent { get { return percent / (NumLines + NumBaselines); } }
		double percent;
		public OperatingMode OperatingMode = OperatingMode.Counter;
		public event EventHandler <ConnectionEventArgs> Connected;
		public event EventHandler <SweepUpdateEventArgs> SweepUpdate;

		public XC ()
		{
			_readThread = new Thread (ReadThread);
			_timer = new Timer (TimerHit);
		}

		public bool IsConnected
		{
			get { return _connected; }
		}

		public void Connect(string comport)
		{
			_connected = false;
			if (0 != libxc.xc_connect (comport))
				return;
			if (0 == libxc.xc_get_properties ()) {
				Counts = new List<ulong>[NumLines];
				Autocorrelations = new ulong[NumLines * DelaySize];
				Crosscorrelations = new ulong[NumBaselines * (1+DelaySize*2)];
				for (int l = 0; l < NumLines; l++) {
					Counts [l] = new List<ulong> ();
				}
				_connected = true;
				_readThread.Start ();
				_timer.Change (0, 1000);
			}
			if (Connected != null)
				Connected (this, new ConnectionEventArgs (IsConnected));
		}

		public void Disconnect()
		{
			libxc.xc_disconnect ();
			ThreadsRunning = false;
			if(((int)_readThread.ThreadState &((int)(ThreadState.Aborted|ThreadState.Stopped|ThreadState.Suspended|ThreadState.Unstarted))) == 0)
				_readThread.Join ();
		}

		void TimerHit(object state)
		{
			if (SweepUpdate != null && Counts != null && Autocorrelations != null && Crosscorrelations != null) {
				if (!Reading)
					return;
				if (OperatingMode == OperatingMode.Counter) {
					for (int l = 0; l < NumLines; l++) {
						if (Counts [l] != null)
							SweepUpdate (this, new SweepUpdateEventArgs (l, Counts [l], OperatingMode));
					}
				} else if (OperatingMode == OperatingMode.Autocorrelator) {
					for (int l = 0; l < NumLines; l++) {
						ulong[] autocorrelations = new ulong[DelaySize];
						Array.Copy (Autocorrelations, l * DelaySize, autocorrelations, 0, DelaySize);
						SweepUpdate (this, new SweepUpdateEventArgs (l, autocorrelations.ToList (), OperatingMode));
					}
				} else if (OperatingMode == OperatingMode.Crosscorrelator) {
					for (int i = 0; i < NumBaselines; i++) {
						ulong[] crosscorrelations = new ulong[DelaySize * 2 + 1];
						Array.Copy (Crosscorrelations, i * (DelaySize * 2 + 1), crosscorrelations, 0, DelaySize * 2 + 1);
						SweepUpdate (this, new SweepUpdateEventArgs (i, crosscorrelations.ToList (), OperatingMode));
					}
				}
			}
		}

		void ReadThread()
		{
			ThreadsRunning = true;
			ulong[] counts = new ulong[NumLines];
			while(ThreadsRunning) {
				percent = 0;
				if (OperatingMode == OperatingMode.Counter) {
					libxc.xc_get_packet (counts, null, null);
					for (int l = 0; l < NumLines; l++) {
						Counts [l].Add (counts [l]);
					}
				} else if (OperatingMode == OperatingMode.Autocorrelator) {
					libxc.xc_scan_autocorrelations (Autocorrelations, ref percent, ref _Reading);
				} else if (OperatingMode == OperatingMode.Crosscorrelator) {
					libxc.xc_scan_crosscorrelations (Crosscorrelations, ref percent, ref _Reading);
				}
			}
		}

		public void EnableCapture()
		{
			if (Reading)
				return;
			libxc.xc_enable_capture (1);
			Reading = true;
		}

		public void DisableCapture()
		{
			libxc.xc_enable_capture (0);
			Reading = false;
		}

		public void SetBaudRate(baud_rate rate, bool setterm = true)
		{
			libxc.xc_set_baudrate (rate, setterm ? 1 : 0);
		}

		public void SetLine(int line, bool lv, bool hv)
		{
			libxc.xc_set_power (line, lv, hv);
		}

		public void SetFrequencyDivider(byte divider)
		{
			libxc.xc_set_frequency_divider (divider);
		}
	}
}

