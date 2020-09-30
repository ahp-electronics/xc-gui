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
		public extern static void xc_scan_autocorrelations(correlation[] autocorrelations, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static void xc_scan_crosscorrelations(correlation[] crosscorrelations, ref double percent, ref int interrupt);
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

	[StructLayout(LayoutKind.Sequential)]
	public struct correlation {
		public ulong correlations;
		public ulong counts;
		public double coherence;
	};

	public enum OperatingMode {
		Counter = 0,
		Autocorrelator = 1,
		Crosscorrelator = 2,
	};

	public class PacketReceivedEventArgs : EventArgs
	{
		public uint[] Counts;
		public uint[] Lines;
		public uint[] Correlations;
		public PacketReceivedEventArgs(uint [] counts, uint [] lines,uint [] correlations)
		{
			Counts = counts;
			Lines = lines;
			Correlations = correlations;
		}
	}

	public class SweepUpdateEventArgs : EventArgs
	{
		public List<double> Counts;
		public int Index;
		public OperatingMode Mode;
		public SweepUpdateEventArgs(int index, List<double> counts, OperatingMode mode)
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
		public int FrameNumber { get; set; }

		public List<double>[] Counts;
		correlation[] Autocorrelations;
		correlation[] Crosscorrelations;

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
				Counts = new List<double>[NumLines];
				Autocorrelations = new correlation[NumLines * DelaySize];
				Crosscorrelations = new correlation[NumBaselines * (1+DelaySize*2)];
				for (int l = 0; l < NumLines; l++) {
					Counts [l] = new List<double> ();
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
						double[] counts = new double[Counts [l].Count];
						Array.Copy (Counts [l].ToArray (), counts, Counts [l].Count);
						for (int x = 0; x < counts.Length; x++) {
							try {
								counts [x] /= PacketTime;
							} catch {
							}
						}
						if (Counts [l] != null)
							SweepUpdate (this, new SweepUpdateEventArgs (l, counts.ToList (), OperatingMode));
					}
				} else if (OperatingMode == OperatingMode.Autocorrelator) {
					for (int i = 0; i < NumLines; i++) {
						double[] correlations = new double[DelaySize];
						for (int x = 1, y = i * DelaySize + 1; x < DelaySize; x++, y++) {
							try {
								correlations [x] = (double)Autocorrelations [y].coherence;
							} catch {
							}
						}
						SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), OperatingMode));
					}
				} else if (OperatingMode == OperatingMode.Crosscorrelator) {
					for (int i = 0; i < NumBaselines; i++) {
						double[] correlations = new double[DelaySize * 2 + 1];
						for (int x = 1, y = i * (DelaySize * 2 + 1) + 1; x < DelaySize * 2 + 1; x++, y++) {
							try {
								correlations [x] = (double)Crosscorrelations [y].coherence;
							} catch {
							}
						}
						SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), OperatingMode));
					}
				}
			}
		}

		void ReadThread()
		{
			ThreadsRunning = true;
			while(ThreadsRunning) {
				percent = 0;
				if (OperatingMode == OperatingMode.Counter) {
					ulong[] counts = new ulong[NumLines];
					libxc.xc_get_packet (counts, null, null);
					for (int l = 0; l < NumLines; l++) {
						Counts [l].Add (counts [l]);
					}
				} else if (OperatingMode == OperatingMode.Autocorrelator) {
					libxc.xc_scan_autocorrelations (Autocorrelations, ref percent, ref _Reading);
				} else if (OperatingMode == OperatingMode.Crosscorrelator) {
					libxc.xc_scan_crosscorrelations (Crosscorrelations, ref percent, ref _Reading);
				}
				FrameNumber++;
			}
		}

		public void EnableCapture()
		{
			if (Reading)
				return;
			libxc.xc_enable_capture (1);
			Reading = true;
			FrameNumber = 1;
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

