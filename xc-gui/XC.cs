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
		public extern static int ahp_xc_get_autocorrelator_jittersize();
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_crosscorrelator_jittersize();
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
		public extern static void ahp_xc_set_baudrate(baud_rate rate);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_leds(int index, int leds);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_delay(int index, byte value);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_line(int index, byte value);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_set_frequency_divider(byte value);
		[DllImport("ahp_xc")]
		public extern static int ahp_xc_get_frequency_divider();
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_scan_autocorrelations(correlation[] autocorrelations, int stacksize, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static void ahp_xc_scan_crosscorrelations(correlation[] crosscorrelations, int stacksize, ref double percent, ref int interrupt);
		[DllImport("ahp_xc")]
		public extern static bool ahp_xc_get_packet(ulong[] counts, correlation[] autocorrelations, correlation[] correlations);
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

	public enum OperatingMode {
		Counter = 1,
		Autocorrelator = 2,
		Crosscorrelator = 3,
	};
	public enum ModeFlags {
		Log = 4,
		Live = 8,
		Inverse = 16,
	};

	[StructLayout(LayoutKind.Sequential)]
	public struct correlation {
		public ulong correlations;
		public ulong counts;
		public double coherence;
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
		public int Mode;
		public SweepUpdateEventArgs(int index, List<double> counts, int mode)
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

		const int Live = 1 << 1;
		const int Log = 1 << 2;
		const int Inverse = 1 << 3;

		public int PacketSize  { get { return  Native.ahp_xc_get_packetsize(); } }
		public int TimeScale  { get { return  Native.ahp_xc_get_frequency_divider(); }
			set { 
				bool tmp = Reading;
				Reading = false;
				Native.ahp_xc_set_frequency_divider ((byte)value);
				if (0 == (Mode & (int)ModeFlags.Live)) {
					Array.Resize (ref Autocorrelations, NumLines * DelaySize * (value + 2) / 2);
					Array.Resize (ref Crosscorrelations, NumBaselines * (DelaySize * (value + 2) + 1));
				}
				Reading = Reading;
			}
		}
		public double PacketTime  { get { return (double) Native.ahp_xc_get_packettime() / 2000000.0; } }
		public int NumLines { get { return  Native.ahp_xc_get_nlines(); } }
		public int NumBaselines { get { return  Native.ahp_xc_get_nbaselines(); } }
		public int ClockFrequency { get { return  Native.ahp_xc_get_frequency(); } }
		public int BitsPerSample { get { return  Native.ahp_xc_get_bps(); } }
		public int SpectraSize { get { return  Native.ahp_xc_get_autocorrelator_jittersize(); } }
		public int CorrelatorSize { get { return  Native.ahp_xc_get_crosscorrelator_jittersize(); } }
		public int DelaySize { get { return  Native.ahp_xc_get_delaysize(); } }
		public int BaudRate { get { return  Native.ahp_xc_get_baudrate(); } }
		public int FrameNumber { get; set; }
		public int Stack { get; set; }

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
		public int Mode = (int)OperatingMode.Counter;
		public event EventHandler <ConnectionEventArgs> Connected;
		public event EventHandler <SweepUpdateEventArgs> SweepUpdate;

		public XC ()
		{
			_readthread = new Thread (ReadThread);
			_timer = new Timer (TimerHit);
			Stack = 20;
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
				Autocorrelations = new correlation[NumLines * (DelaySize+SpectraSize-1) * (TimeScale + 2) / 2];
				Crosscorrelations = new correlation[NumBaselines * ((DelaySize+CorrelatorSize-1)*2-1) * (TimeScale + 2) / 2];
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
			if (Counts != null && Autocorrelations != null && Crosscorrelations != null) {
				if (!Reading)
					return;
				if (0 == (Mode & (int)ModeFlags.Live)) {
					if((Mode&3) == (int)OperatingMode.Counter) {
					} else if((Mode&3) == (int)OperatingMode.Autocorrelator) {
						double[] correlations = new double[Autocorrelations.Length / NumLines];
						double[] correlations_log = new double[Autocorrelations.Length / NumLines];
						complex[] spectrum = new complex[correlations.Length];
						for (int i = 0; i < NumLines; i++) {
							for (int x = 1, y = i * correlations.Length + 2; x < correlations.Length - 1; x++, y++) {
								try {
									correlations [x] = Math.Max (0, Autocorrelations [y].coherence);
									correlations_log [x] = Math.Max (0, Math.Pow (Autocorrelations [y].coherence, 0.5));
									spectrum [x].real = Autocorrelations [y].coherence;
									spectrum [x].imaginary = Autocorrelations [y].coherence;
								} catch {
								}
							}
							if (SweepUpdate != null) {
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), (int)OperatingMode.Autocorrelator));
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations_log.ToList (), (int)OperatingMode.Autocorrelator | (int)ModeFlags.Log));
							}
							IntPtr plan = Native.fftw_plan_dft_c2r (1, new int[] { correlations.Length }, spectrum, correlations, 0);
							Native.fftw_execute (plan);
							Native.fftw_destroy_plan (plan);
							for (int x = 0; x < correlations.Length / 2; x++) {
								double tmp = correlations [x];
								correlations [x] = correlations [x + correlations.Length / 2];
								correlations [x + correlations.Length / 2] = tmp;
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), (int)OperatingMode.Autocorrelator | (int)ModeFlags.Inverse));
						}
					} else if((Mode&3) == (int)OperatingMode.Crosscorrelator) {
						for (int i = 0; i < NumBaselines; i++) {
							double[] correlations = new double[Crosscorrelations.Length / NumBaselines];
							double[] correlations_log = new double[correlations.Length];
							complex[] spectrum = new complex[correlations.Length];
							for (int x = 0, y = i * correlations.Length; x < correlations.Length; x++, y++) {
								try {
									correlations [x] = Crosscorrelations [y].coherence;
									correlations_log [x] = Math.Pow (correlations [x], 0.5);
									spectrum [x].real = correlations [x];
									spectrum [x].imaginary = correlations [x];
								} catch {
								}
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), (int)OperatingMode.Crosscorrelator));
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations_log.ToList (), (int)OperatingMode.Crosscorrelator | (int)ModeFlags.Log));
							IntPtr plan = Native.fftw_plan_dft_c2r (1, new int[] { correlations.Length }, spectrum, correlations, 0);
							Native.fftw_execute (plan);
							Native.fftw_destroy_plan (plan);
							for (int x = 0; x < correlations.Length / 2; x++) {
								double tmp = correlations [x];
								correlations [x] = correlations [x + correlations.Length / 2];
								correlations [x + correlations.Length / 2] = tmp;
							}
							if (SweepUpdate != null)
								SweepUpdate (this, new SweepUpdateEventArgs (i, correlations.ToList (), (int)OperatingMode.Crosscorrelator | (int)ModeFlags.Inverse));
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
					if (SweepUpdate == null)
						continue;
					ulong[] counts = new ulong[NumLines];
					correlation[] autocorrelations = new correlation[NumLines*SpectraSize];
					correlation[] crosscorrelations = new correlation[NumBaselines*(CorrelatorSize*2-1)];
					if (!Native.ahp_xc_get_packet (counts, autocorrelations, crosscorrelations)) {
						int idx = 0;
						for (int l = 0; l < NumLines; l++) {
							if(0 != (Mode & (int)ModeFlags.Live)) {
								if((Mode&3) == (int)OperatingMode.Counter) {
									Counts [l].Add (counts [l]);
									SweepUpdate (this, new SweepUpdateEventArgs (l, Counts[l], (int)OperatingMode.Counter));
								}
								if (counts [l] == 0)
									continue;
								double[] correlations = new double[autocorrelations.Length / NumLines];
								double[] correlations_log = new double[correlations.Length];
								complex[] spectrum = new complex[correlations.Length];
								for (int i = 1; i < SpectraSize; i++) {
									correlations[i] += (double)autocorrelations[l*SpectraSize+i].coherence;
									correlations_log [i] += Math.Pow (correlations [i], 0.5);
									spectrum [i].real += correlations [i];
									spectrum [i].imaginary += correlations [i];
								}
								if(FrameNumber == 1) {
									for (int i = 1; i < SpectraSize; i++) {
										correlations[i] /= (double)Stack;
										correlations_log [i] /= (double)Stack;
										spectrum [i].real /= (double)Stack;
										spectrum [i].imaginary /= (double)Stack;
									}
									if((Mode&3) == (int)OperatingMode.Autocorrelator) {
										SweepUpdate (this, new SweepUpdateEventArgs (l, correlations.ToList (), (int)OperatingMode.Autocorrelator|(int)ModeFlags.Live));
										if(0 != (Mode&(int)ModeFlags.Log))
											SweepUpdate (this, new SweepUpdateEventArgs (l, correlations_log.ToList (), (int)OperatingMode.Autocorrelator|(int)ModeFlags.Log));
										if(0 != (Mode&(int)ModeFlags.Inverse)) {
											IntPtr plan = Native.fftw_plan_dft_c2r (1, new int[] { correlations.Length }, spectrum, correlations, 0);
											Native.fftw_execute (plan);
											Native.fftw_destroy_plan (plan);
											for (int x = 0; x < correlations.Length / 2; x++) {
												double tmp = correlations [x];
												correlations [x] = correlations [x + correlations.Length / 2];
												correlations [x + correlations.Length / 2] = tmp;
											}
											SweepUpdate (this, new SweepUpdateEventArgs (l, correlations.ToList (), (int)OperatingMode.Autocorrelator|(int)ModeFlags.Inverse));
										}
									}
									for (int i = 1; i < SpectraSize; i++) {
										correlations[i] = 0;
										correlations_log [i] = 0;
										spectrum [i].real = 0;
										spectrum [i].imaginary = 0;
									}
								}
								for (int i = l+1; i < NumLines; i++) {
									double[] _correlations = new double[crosscorrelations.Length / NumBaselines];
									double[] _correlations_log = new double[_correlations.Length];
									complex[] _spectrum = new complex[_correlations.Length];
									for (int x = 0; x < CorrelatorSize*2-1; x++) {
										_correlations[x] += (double)crosscorrelations[idx*(CorrelatorSize*2-1)+x].coherence;
										_correlations_log [x] += Math.Pow (_correlations [x], 0.5);
										_spectrum [x].real += _correlations [x];
										_spectrum [x].imaginary += _correlations [x];
									}
									if(FrameNumber == 1) {
										for (int x = 0; x < CorrelatorSize*2-1; x++) {
											_correlations[x] /= (double)Stack;
											_correlations_log [x] /= (double)Stack;
											_spectrum [x].real /= (double)Stack;
											_spectrum [x].imaginary /= (double)Stack;
										}
										if(0 != (Mode&(int)OperatingMode.Crosscorrelator)) {
											SweepUpdate (this, new SweepUpdateEventArgs (l, _correlations.ToList (), (int)OperatingMode.Crosscorrelator|(int)ModeFlags.Live));
											if(0 != (Mode&(int)ModeFlags.Log))
												SweepUpdate (this, new SweepUpdateEventArgs (l, _correlations_log.ToList (), (int)OperatingMode.Crosscorrelator|(int)ModeFlags.Log));
											if(0 != (Mode&(int)ModeFlags.Inverse)) {
												IntPtr _plan = Native.fftw_plan_dft_c2r (1, new int[] { _correlations.Length }, _spectrum, _correlations, 0);
												Native.fftw_execute (_plan);
												Native.fftw_destroy_plan (_plan);
												for (int x = 0; x < _correlations.Length / 2; x++) {
													double tmp = _correlations [x];
													_correlations [x] = _correlations [x + _correlations.Length / 2];
													_correlations [x + _correlations.Length / 2] = tmp;
												}
												SweepUpdate (this, new SweepUpdateEventArgs (l, _correlations.ToList (), (int)OperatingMode.Crosscorrelator|(int)ModeFlags.Inverse));
											}
										}
										for (int x = 0; x < CorrelatorSize*2-1; x++) {
											_correlations[x] = 0;
											_correlations_log [x] = 0;
											_spectrum [x].real = 0;
											_spectrum [x].imaginary = 0;
										}
									}
								}
							} else {
								if((Mode&3) == (int)OperatingMode.Counter) {
									Counts [l].Add (counts [l]);
								} else if((Mode&3) == (int)OperatingMode.Autocorrelator) {
									Native.ahp_xc_scan_autocorrelations (Autocorrelations, Stack, ref percent, ref _Reading);
								} else if((Mode&3) == (int)OperatingMode.Crosscorrelator) {
									Native.ahp_xc_scan_crosscorrelations (Crosscorrelations, Stack, ref percent, ref _Reading);
								}
							}
						}
					}
				} catch(Exception ex){
					Console.WriteLine (ex.Message + Environment.NewLine + ex.StackTrace);
				}
				FrameNumber %= Stack;
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
			 Native.ahp_xc_set_baudrate (rate);
		}

		public void SetLine(int line, bool lv, bool hv, bool invertauto = false, bool invertcross = false)
		{
			Native.ahp_xc_set_leds (line, ((lv ? 1 : 0)|(hv ? 2 : 0)|(invertauto ? 4 : 0)|(invertcross ? 8 : 0)));
		}
	}
}

