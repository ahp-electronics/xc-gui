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
	[StructLayout(LayoutKind.Sequential)]
	public struct complex {
		public double real;
		public double imaginary;
	};

	class Native {
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_baudrate();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_bps();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_nlines();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_nbaselines();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_delaysize();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_frequency();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_packetsize();
		[DllImport("ahp_xc")]
		public extern static uint ahp_xc_get_packettime();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_open(int fd);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_close();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_connect(string port);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_disconnect();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_properties();
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_enable_capture(int enable);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_select_input(int index);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_baudrate(baud_rate rate, int setterm);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_power(int index, bool lv, bool hv);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_delay(int index, byte value);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_line(int index, byte value);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_frequency_divider(byte value);
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_frequency_divider();
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_scan_autocorrelations(correlation[] autocorrelations, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_scan_crosscorrelations(correlation[] crosscorrelations, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_packet(ulong[] counts, ulong[] autocorrelations, ulong[] correlations);
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_send_command(it_cmd c, byte value);
		[DllImport("fftw3")]
		public extern static IntPtr fftw_plan_dft_r2c(int rank, int[] n, double[] input, complex[] output, uint flags);
		[DllImport("fftw3")]
		public extern static IntPtr fftw_plan_dft_c2r(int rank, int[] n, complex[] input, double[] output, uint flags);
		[DllImport("fftw3")]
		public extern static void fftw_execute(IntPtr plan);
		[DllImport("fftw3")]
		public extern static void fftw_destroy_plan(IntPtr plan);
		[DllImport("fftw3")]
		public extern static void fftw_cleanup();
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
		InverseAutocorrelator = 3,
		InverseCrosscorrelator = 4,
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
		public int PacketSize  { get { return  Native.ahp_xc_get_packetsize(); } }
		public int TimeScale  { get { return  Native.ahp_xc_get_frequency_divider(); } set { 
				bool tmp = Reading;
				Reading = false;
				 Native.ahp_xc_set_frequency_divider ((byte)value);
				Array.Resize (ref Autocorrelations, NumLines * DelaySize * (value + 2) / 2);
				Reading = Reading;
			}
		}
		public double PacketTime  { get { return (double) Native.ahp_xc_get_packettime() / 2000000.0; } }
		public int NumLines { get { return  Native.ahp_xc_get_nlines(); } }
		public int NumBaselines { get { return  Native.ahp_xc_get_nbaselines(); } }
		public int ClockFrequency { get { return  Native.ahp_xc_get_frequency(); } }
		public int BitsPerSample { get { return  Native.ahp_xc_get_bps(); } }
		public int DelaySize { get { return  Native.ahp_xc_get_delaysize(); } }
		public int BaudRate { get { return  Native.ahp_xc_get_baudrate(); } }
		public int FrameNumber { get; set; }

		public List<double>[] Counts;
		correlation[] Autocorrelations;
		correlation[] Crosscorrelations;

		Timer _timer;
		Thread _readthread;
		bool ThreadsRunning;
		bool _connected;
		bool Reading { get { return (_Reading == 0); } set { _Reading = (value ? 0 : 1); } } 
		int _Reading = 1;
		public double Percent { get { return percent; } }
		double percent;
		public OperatingMode OperatingMode = OperatingMode.Counter;
		public event EventHandler <ConnectionEventArgs> Connected;
		public event EventHandler <SweepUpdateEventArgs> SweepUpdate;

		public XC ()
		{
			_readthread = new Thread (ReadThread);
			_timer = new Timer (TimerHit);
		}

		public bool IsConnected
		{
			get { return _connected; }
		}

		public void Connect(string comport)
		{
			_connected = false;
			int err =  Native.ahp_xc_connect (comport);
			if (0 != err)
				return;
			err =  Native.ahp_xc_get_properties ();
			if (0 == err) {
				Counts = new List<double>[NumLines];
				Autocorrelations = new correlation[NumLines * DelaySize * (TimeScale + 2) / 2];
				Crosscorrelations = new correlation[NumBaselines * (1+DelaySize*2)];
				for (int l = 0; l < NumLines; l++) {
					Counts [l] = new List<double> ();
				}
				_connected = true;
				ThreadsRunning = true;
				_readthread.IsBackground = true;
				_readthread.Start();
				_timer.Change (0, 1000);
			}
			if (Connected != null)
				Connected (this, new ConnectionEventArgs (IsConnected));
		}

		public void Disconnect()
		{
			Reading = false;
			ThreadsRunning = false;
			_timer.Change (0, Timeout.Infinite);
			_timer.Dispose ();
			 Native.ahp_xc_disconnect ();
		}

		void TimerHit(object state)
		{
			if (SweepUpdate != null && Counts != null && Autocorrelations != null && Crosscorrelations != null) {
				if (!Reading)
					return;
				if (OperatingMode == OperatingMode.Counter) {
					for (int l = 0; l < NumLines; l++) {
						if (Counts [l].Count > 0) {
							double[] counts = Counts [l].ToArray();
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (l, counts.ToList (), OperatingMode.Counter));
						}
					}
				}
			}
		}

		void ReadThread()
		{
			ThreadsRunning = true;
			while (ThreadsRunning) {
				try {
					percent = 0;
					if (OperatingMode == OperatingMode.Counter) {
						ulong[] counts = new ulong[NumLines];
						if (0 ==  Native.ahp_xc_get_packet (counts, null, null)) {
							for (int l = 0; l < NumLines; l++) {
								Counts [l].Add (counts [l]);
							}
						} else {
							for (int l = 0; l < NumLines; l++) {
								Counts [l].Add (Counts [l].Last ());
							}
						}
					} else if (OperatingMode == OperatingMode.Autocorrelator) {
						Native.ahp_xc_scan_autocorrelations (Autocorrelations, ref percent, ref _Reading);
						double[] correlations = new double[Autocorrelations.Length / NumLines];
						complex[] spectrum = new complex[correlations.Length];
						for (int i = 0; i < NumLines; i++) {
							for (int x = 0, y = i * correlations.Length + 2; x < correlations.Length - 1; x++, y++) {
								try {
									correlations [x] = Math.Pow(Autocorrelations [y].coherence, 0.1);
									spectrum [x].real = Autocorrelations [y].coherence;
									spectrum [x].imaginary = Autocorrelations [y].coherence;
								} catch {
								}
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), OperatingMode.Autocorrelator));
							IntPtr plan =  Native.fftw_plan_dft_c2r (1, new int[] { correlations.Length }, spectrum, correlations, 0);
							Native.fftw_execute (plan);
							Native.fftw_destroy_plan (plan);
							for (int x = 0; x < correlations.Length / 2; x++) {
								double tmp = correlations [x];
								correlations [x] = correlations [x + correlations.Length / 2];
								correlations [x + correlations.Length / 2] = tmp;
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), OperatingMode.InverseAutocorrelator));
						}
					} else if (OperatingMode == OperatingMode.Crosscorrelator) {
						Native.ahp_xc_scan_crosscorrelations (Crosscorrelations, ref percent, ref _Reading);
						for (int i = 0; i < NumBaselines; i++) {
							double[] correlations = new double[DelaySize * 2 + 1];
							complex[] spectrum = new complex[DelaySize * 2 + 1];
							for (int x = 0, y = i * (DelaySize * 2 + 1); x < DelaySize * 2 + 1; x++, y++) {
								try {
									correlations [x] = Crosscorrelations [y].coherence;
									spectrum [x].real = Crosscorrelations [y].coherence;
									spectrum [x].imaginary = Crosscorrelations [y].coherence;
								} catch {
								}
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), OperatingMode.Crosscorrelator));
							IntPtr plan =  Native.fftw_plan_dft_c2r (1, new int[] { DelaySize * 2 + 1 }, spectrum, correlations, 0);
							 Native.fftw_execute (plan);
							 Native.fftw_destroy_plan (plan);
							for (int x = 0; x < DelaySize; x++) {
								double tmp = correlations [x];
								correlations [x] = correlations [x + DelaySize + 1];
								correlations [x + DelaySize + 1] = tmp;
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), OperatingMode.InverseCrosscorrelator));
						}
					}
				} catch {
				}
				FrameNumber++;
			}
		}

		public void EnableCapture()
		{
			if (Reading)
				return;
			 Native.ahp_xc_enable_capture (1);
			Reading = true;
			FrameNumber = 1;
		}

		public void DisableCapture()
		{
			 Native.ahp_xc_enable_capture (0);
			Reading = false;
		}

		public void SetBaudRate(baud_rate rate, bool setterm = true)
		{
			 Native.ahp_xc_set_baudrate (rate, setterm ? 1 : 0);
		}

		public void SetLine(int line, bool lv, bool hv)
		{
			 Native.ahp_xc_set_power (line, lv, hv);
		}
	}
}

