﻿using System;
using System.Windows.Forms;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms.DataVisualization;
using System.Collections.Generic;
using System.Linq;
using System.IO.Ports;
using System.Threading.Tasks;
using System.Threading;

namespace Crosscorrelator
{
	public class MainForm : Form
	{
		Label label;
		Panel panel = new Panel ();
		Button start = new Button ();
		Button stop = new Button ();
		Graph[] chart;
		Graph charts;
		ComboBox[] line;
		NumericUpDown freqdiv;
		XC correlator;
		double TimeScale = 10.0;
		ProgressBar progress = new ProgressBar();

		public MainForm ()
		{
			correlator = new XC ();
			correlator.Connected += Correlator_Connected;
			this.ClientSize = new Size (720, 480);
			this.Shown += MainForm_Shown;
			System.Threading.Timer timer = new System.Threading.Timer (TimerHit);
			timer.Change (0, 1000);
		}

		void Correlator_Connected (object sender, ConnectionEventArgs e)
		{
			if (e.Connected) {
				correlator.SweepUpdate += Correlator_SweepUpdate;
				panel = new Panel ();
				panel.AutoScroll = true;
				panel.Size = new Size (720, 480 - (33 + 30 * (correlator.NumLines / 4)));
				panel.Location = new Point (0, 33 + 30 * (correlator.NumLines / 4));
				Controls.Add (panel);
				chart = new Graph[correlator.NumBaselines];

				counts = new double[correlator.NumLines][];
				lines = new double[correlator.NumLines][];
				line = new ComboBox[correlator.NumLines];
				Dark = new double[correlator.NumLines];
				AutoDark = new Dictionary<double, double>[correlator.NumLines];
				CrossDark = new Dictionary<double, double>[correlator.NumBaselines];
				for (int x = 0; x < correlator.NumLines; x++) {
					AutoDark [x] = new Dictionary<double, double> ();
					label = new Label ();
					label.Text = "Line " + (x + 1);
					label.Size = new Size (40, 18);
					label.Location = new Point (20 + 160 * (x % 4), 35 + (30 * (x / 4)));
					Controls.Add (label);
					Button b = new Button ();
					b.Size = new Size (20, 20);
					b.Location = new Point (60 + 160 * (x % 4), 35 + (30 * (x / 4)));
					b.Name = "D" + x.ToString ("D2");
					b.BackColor = Color.Green;
					b.Click += button_Click;
					Controls.Add (b);
					line[x] = new ComboBox ();
					line[x].DropDownStyle = ComboBoxStyle.DropDownList;
					line[x].Size = new Size (75, 23);
					line[x].Location = new Point (80 + 160 * (x % 4), 33 + (30 * (x / 4)));
					line[x].Name = "line" + x;
					line[x].Text = "Line " + (x + 1);
					line[x].Items.AddRange (new object[] { "Off", "On", "On,Power", "Power" });
					line[x].SelectedIndexChanged += Line_SelectedIndexChanged;
					line[x].SelectedIndex = 0;
					Controls.Add (line[x]);
					counts [x] = new double[60];
					lines [x] = new double[correlator.DelaySize];
				}
				charts = new Graph ();
				charts.Size = new Size (panel.Width-30, panel.Height);;
				charts.Location = new Point (0, 0);
				charts.StartX = 1;
				charts.StartY = 0;
				charts.EndX = TimeScale;
				charts.EndY = 1.0;
				for (int x = 0; x < correlator.NumBaselines; x++) {
					CrossDark [x] = new Dictionary<double, double> ();
					chart [x] = new Graph ();
					chart [x].BackColor = Color.Transparent;
					chart [x].ShowScale = false;
					chart [x].DrawAverage = false;
					chart [x].Size = new Size (panel.Width-30, panel.Height);;
					chart [x].Location = new Point (0, 0);
					chart [x].StartX = 1;
					chart [x].StartY = 0;
					chart [x].EndX = TimeScale;
					chart [x].EndY = 1.0;
					if (x == 0) {
						charts.Controls.Add (chart [x]);
					} else {
						chart [x - 1].Controls.Add (chart [x]);
					}
				}
				this.Invoke ((MethodInvoker)delegate {
					charts.Invalidate(true);
				});
				start = new Button ();
				start.Size = new Size (80, 23);
				start.Location = new Point (640, 3);
				start.Text = "Run";
				start.TextAlign = ContentAlignment.MiddleCenter;
				start.Click += button_Click;
				Controls.Add (start);
				ComboBox mode;
				stop = new Button ();
				stop.Size = new Size (80, 23);
				stop.Location = new Point (640, 29);
				stop.Text = "Stop";
				stop.TextAlign = ContentAlignment.MiddleCenter;
				stop.Click += button_Click;
				Controls.Add (stop);
				mode = new ComboBox ();
				mode.DropDownStyle = ComboBoxStyle.DropDownList;
				mode.Size = new Size (75, 23);
				mode.Location = new Point (640, 55);
				mode.Items.AddRange (new object[] { "Counter", "Autocorrelator", "Crosscorrelator" });
				mode.SelectedIndexChanged += (object s, EventArgs ev) => {
					correlator.OperatingMode = (OperatingMode)(((ComboBox)s).SelectedIndex);

					switch(correlator.OperatingMode) {
					case OperatingMode.Counter:
						for (int x = 0; x < correlator.NumLines; x++) {
							int ix = x * 256 / correlator.NumLines;
							Pen pen = new Pen (Color.FromArgb ((ix >= 0 && ix < 128) ? ix + 128 : 0, (ix >= 64 && ix < 192) ? ix + 64 : 0, (ix >= 128 && ix < 256) ? ix : 0));
							chart[x].DotsPen = pen;
							chart [x].StartX = 0;
							chart [x].EndX = 10;
							chart [x].EndY = 1.2;
							chart [x].StartY = -0.2;
						}
						charts.StartX = 0;
						charts.EndX = 10;
						charts.EndY = 1.2;
						charts.StartY = -0.2;
						charts.LabelX = "Time (s)";
						charts.LabelY = "Counts";
						break;
					case OperatingMode.Autocorrelator:
						for (int x = 0; x < correlator.NumLines; x++) {
							int ix = x * 256 / correlator.NumLines;
							Pen pen = new Pen (Color.FromArgb ((ix >= 0 && ix < 128) ? ix + 128 : 0, (ix >= 64 && ix < 192) ? ix + 64 : 0, (ix >= 128 && ix < 256) ? ix : 0));
							chart[x].DotsPen = pen;
							chart [x].StartX = 0;
							chart [x].EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
							chart [x].EndY = 1.2;
							chart [x].StartY = -0.2;
						}
						charts.StartX = 0;
						charts.EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
						charts.EndY = 1.2;
						charts.StartY = -0.2;
						charts.LabelX = "Delay (ns)";
						charts.LabelY = "Coherence ratio 0/n";
						break;
					case OperatingMode.Crosscorrelator:
						for (int x = 0; x < correlator.NumBaselines; x++) {
							int ix = x * 256 / correlator.NumBaselines;
							Pen pen = new Pen (Color.FromArgb ((ix >= 0 && ix < 128) ? ix + 128 : 0, (ix >= 64 && ix < 192) ? ix + 64 : 0, (ix >= 128 && ix < 256) ? ix : 0));
							chart[x].DotsPen = pen;
							chart [x].StartX = -(correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
							chart [x].EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
							chart [x].EndY = 2.2;
							chart [x].StartY = -0.2;
						}
						charts.StartX = -(correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
						charts.EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
						charts.EndY = 2.2;
						charts.StartY = -0.2;
						charts.LabelX = "Lag (ns)";
						charts.LabelY = "Coherence ratio n1/n2";
						break;
					}
					for(int x = 0; x < chart.Length; x++) {
						chart[x].Dots.Clear();
					}
					charts.Invalidate(true);
				};
				panel.Controls.Add (charts);
				mode.SelectedIndex = 0;
				Controls.Add (mode);
				label = new Label ();
				label.Size = new Size (75, 23);
				label.Location = new Point (485, 3);
				label.Text = "Timescale";
				Controls.Add (label);
				freqdiv = new NumericUpDown ();
				freqdiv.Size = new Size (75, 23);
				freqdiv.Location = new Point (560, 3);
				freqdiv.Minimum = 0;
				freqdiv.Maximum = 63;
				freqdiv.ValueChanged += Freqdiv_ValueChanged;
				freqdiv.Value = 0;
				Controls.Add (freqdiv);
				label = new Label ();
				label.Size = new Size (75, 23);
				label.Location = new Point (325, 3);
				label.Text = "X scale";
				Controls.Add (label);
				NumericUpDown scalex;
				scalex = new NumericUpDown ();
				scalex.Size = new Size (75, 23);
				scalex.Location = new Point (400, 3);
				scalex.Minimum = 1;
				scalex.Maximum = 100;
				scalex.Value = 1;
				scalex.ValueChanged += Scalex_ValueChanged;
				Controls.Add (scalex);
				label = new Label ();
				label.Size = new Size (75, 23);
				label.Location = new Point (165, 3);
				label.Text = "Baudrate";
				Controls.Add (label);
				progress.Style = ProgressBarStyle.Continuous;
				progress.Size = new Size (630, 5);
				progress.Location = new Point (5, 27);
				progress.Minimum = 0;
				progress.Maximum = 100;
				Controls.Add (progress);
				ComboBox baudrate;
				baudrate = new ComboBox ();
				baudrate.DropDownStyle = ComboBoxStyle.DropDownList;
				baudrate.Size = new Size (75, 23);
				baudrate.Location = new Point (240, 3);
				baudrate.Items.AddRange (new object[] { "57600", "115200", "230400" });
				baudrate.SelectedIndexChanged += Baudrate_SelectedIndexChanged;
				baudrate.SelectedIndex = 0;
				Controls.Add (baudrate);
			}
			this.Resize += MainForm_Resize;
			this.FormClosing += MainForm_FormClosing;
		}

		void button_Click (object sender, EventArgs e)
		{
			Button btn = (Button)sender;
			start.Click -= button_Click;
			stop.Click -= button_Click;
			this.Invoke ((MethodInvoker)delegate {
				if (btn.Text == "Run") {
					for (int x = 0; x < chart.Length; x++) {
						chart [x].Dots.Clear ();
					}
					switch (correlator.OperatingMode) {
					case OperatingMode.Autocorrelator:
					case OperatingMode.Crosscorrelator:
						start.Enabled = false;
						break;
					case OperatingMode.Counter:
						start.Text = "Pause";
						break;
					default:break;
					}
					freqdiv.Enabled = false;
					correlator.EnableCapture ();
					nframe = 1;
				} else if (btn.Text == "Pause") {
					correlator.DisableCapture ();
					btn.Text = "Continue";
				} else if (btn.Text == "Continue") {
					correlator.EnableCapture ();
					btn.Text = "Pause";
				} else if (btn.Text == "Stop") {
					correlator.DisableCapture ();
					freqdiv.Enabled = true;
					switch (correlator.OperatingMode) {
					case OperatingMode.Autocorrelator:
					case OperatingMode.Crosscorrelator:
						start.Enabled = true;
						break;
					case OperatingMode.Counter:
						start.Text = "Run";
						break;
					default:break;
					}
				} else if (btn.Name.Substring (0, 1) == "D") {
					if (btn.BackColor == Color.Red) {
						int idx = Int32.Parse (btn.Name.Substring (1, 2));
						switch (correlator.OperatingMode) {
						case OperatingMode.Autocorrelator:
							AutoDark [idx].Clear ();
							break;
						case OperatingMode.Counter:
							Dark [idx] = 0;
							break;
						default:
							break;
						}
						btn.BackColor = Color.Green;
					} else {
						int idx = Int32.Parse (btn.Name.Substring (1, 2));
						if(chart [idx].Dots.Count > 0) {
							switch (correlator.OperatingMode) {
							case OperatingMode.Autocorrelator:
								AutoDark [idx].Clear ();
								foreach (KeyValuePair<double, double> pair in chart [idx].Dots) {
									AutoDark [idx].Add(pair.Key, pair.Value);
								}
								break;
							case OperatingMode.Counter:
								Dark [idx] = chart [idx].Dots.Values.Average ();
								break;
							default:
								break;
							}
							btn.BackColor = Color.Red;
						}
					}
				}
				charts.Invalidate (true);
				start.Invalidate (true);
			});
			start.Click += button_Click;
			stop.Click += button_Click;
		}

		void Scalex_ValueChanged (object sender, EventArgs e)
		{
			NumericUpDown updown = (NumericUpDown)sender;
			updown.ValueChanged -= Scalex_ValueChanged;
			if (chart != null) {
				switch (correlator.OperatingMode) {
				case OperatingMode.Counter:
				case OperatingMode.Autocorrelator:
					for (int x = 0; x < correlator.NumLines; x++) {
						chart [x].StartX = 0;
						chart [x].EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency) / (double)updown.Value;
					}
					break;
				case OperatingMode.Crosscorrelator:
					for (int x = 0; x < correlator.NumBaselines; x++) {
						chart [x].StartX = -(correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency) / (double)updown.Value;
						chart [x].EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency) / (double)updown.Value;
					}
					break;
				}
			}
			updown.ValueChanged += Scalex_ValueChanged;
		}

		void MainForm_FormClosing (object sender, FormClosingEventArgs e)
		{
			for (int x = 0; x < correlator.NumLines; x++) {
				correlator.SetLine (x, false, false);
			}
			correlator.DisableCapture ();
			correlator.Disconnect ();
		}

		void MainForm_Resize (object sender, EventArgs e)
		{
			this.Resize -= MainForm_Resize;
			panel.Size = new Size (this.ClientSize.Width, this.ClientSize.Height - (33 + 30 * (correlator.NumLines / 4)));
			for (int x = 0; x < chart.Length; x++) {
				chart [x].Size = new Size (panel.Width-30, panel.Height);
			}
			charts.Size = new Size (panel.Width-30, panel.Height);
			this.Invoke ((MethodInvoker)delegate {
				charts.Invalidate(true);
			});
			this.Resize += MainForm_Resize;
		}

		void Line_SelectedIndexChanged (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.SelectedIndexChanged -= Line_SelectedIndexChanged;
			if(combo.Name.Length > 4)
				correlator.SetLine (Int32.Parse (combo.Name.Substring (4)), combo.SelectedItem.ToString().Contains("On"), combo.SelectedItem.ToString().Contains("Power"));
			combo.SelectedIndexChanged += Line_SelectedIndexChanged;
		}

		void Baudrate_SelectedIndexChanged (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.SelectedIndexChanged -= Baudrate_SelectedIndexChanged;
			correlator.SetBaudRate ((baud_rate)combo.SelectedIndex);
			combo.SelectedIndexChanged += Baudrate_SelectedIndexChanged;
		}

		void Freqdiv_ValueChanged (object sender, EventArgs e)
		{
			NumericUpDown updown = (NumericUpDown)sender;
			updown.ValueChanged -= Freqdiv_ValueChanged;
			correlator.TimeScale = (byte)updown.Value;
			switch (correlator.OperatingMode) {
			case OperatingMode.Autocorrelator:
				for (int x = 0; x < correlator.NumLines; x++) {
					chart [x].StartX = 0;
					chart [x].EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
				}
				charts.StartX = 0;
				charts.EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
				break;
			case OperatingMode.Crosscorrelator:
				for (int x = 0; x < correlator.NumBaselines; x++) {
					chart [x].StartX = -(correlator.DelaySize * 1000000000.0 * Math.Pow(2,(double)correlator.TimeScale) / correlator.ClockFrequency);
					chart [x].EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
				}
				charts.StartX = -(correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
				charts.EndX = (correlator.DelaySize * 1000000000.0 * Math.Pow(2, (double)correlator.TimeScale) / correlator.ClockFrequency);
				break;
			}
			this.Invoke ((MethodInvoker)delegate {
				charts.Invalidate (true);
			});
			updown.ValueChanged += Freqdiv_ValueChanged;
		}

		void MainForm_Shown (object sender, EventArgs e)
		{
			Label label;
			label = new Label ();
			label.Size = new Size (75, 23);
			label.Location = new Point (5, 3);
			label.Text = "Serial port";
			Controls.Add (label);
			ComboBox port = new ComboBox ();
			port.Size = new Size (75, 23);
			port.Location = new Point (80, 3);
			port.Items.AddRange (SerialPort.GetPortNames ());
			port.SelectedIndex = 0;
			port.Click += Port_Click;
			port.SelectedIndexChanged += Port_SelectedIndexChanged;
			Controls.Add (port);
		}
		void TimerHit(object state)
		{
			if (this.IsHandleCreated) {
				this.Invoke ((MethodInvoker)delegate {
					progress.Value = Math.Max(progress.Minimum, Math.Min(progress.Maximum, (int)correlator.Percent));
					progress.Invalidate(true);
				});
			}
		}

		void Port_SelectedIndexChanged (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.SelectedIndexChanged -= Port_SelectedIndexChanged;
			correlator.Connect (combo.SelectedItem.ToString ());
			combo.SelectedIndexChanged += Port_SelectedIndexChanged;
		}

		void Port_Click (object sender, EventArgs e)
		{
			ComboBox combo = (ComboBox)sender;
			combo.Click -= Port_Click;
			combo.Items.AddRange (SerialPort.GetPortNames ());
			combo.Click += Port_Click;
		}

		double nframe;
		double[][]counts, lines;
		public Dictionary<double, double>[] AutoDark, CrossDark;
		double[] Dark;
		void Correlator_SweepUpdate(object sender, SweepUpdateEventArgs e)
		{
			correlator.SweepUpdate -= Correlator_SweepUpdate;
			if (chart == null || e == null) {
				correlator.SweepUpdate += Correlator_SweepUpdate;
				return;
			}
			int index2 = e.Index;
			int index1 = correlator.NumLines;
			while (index2 >= index1) {
				index2 -= index1--;
			}
			index1 = correlator.NumLines - index1;
			if (line [index2].SelectedIndex > 2 || line [index2].SelectedIndex < 1) {
				correlator.SweepUpdate += Correlator_SweepUpdate;
				return;
			}
			if (e.Mode == OperatingMode.Crosscorrelator) {
				if (line [index1].SelectedIndex > 2 || line [index1].SelectedIndex < 1) {
					correlator.SweepUpdate += Correlator_SweepUpdate;
					return;
				}
				int idx = e.Index;
				try {
					var values = e.Counts;
					Parallel.For (0, correlator.DelaySize * 2, delegate(int y) {
						try {
							double time = 1000000000.0 / correlator.ClockFrequency;
							int x = y - (correlator.DelaySize * (correlator.TimeScale + 2) / 2);
							while(x > correlator.DelaySize) {
								x -= correlator.DelaySize / 2;
								time *= 2;
							}
							time *= (double)x;
							if (chart [idx].Dots.ContainsKey (time)) {
								if(CrossDark[idx].ContainsKey(time))
									values [y] -= CrossDark[idx][time];
								chart [idx].Dots [time] *= nframe / ++nframe;
								chart [idx].Dots [time] += values [y] / nframe;
							} else
								chart [idx].Dots.Add (time, values [y]);
						} catch {
						}
					});
				} catch {
				}
			} else if (e.Mode == OperatingMode.Autocorrelator) {
				int idx = e.Index;
				try {
					chart [idx].StartX = 0;
					var values = e.Counts;
					Parallel.For (0, values.Count, delegate(int y) {
						try {
							double time = 1000000000.0 / correlator.ClockFrequency;
							int x = y;
							while(x > correlator.DelaySize) {
								x -= correlator.DelaySize / 2;
								time *= 2;
							}
							time *= (double)x;
							if (chart [idx].Dots.ContainsKey (time)) {
								if(AutoDark[idx].ContainsKey(time))
									values [y] -= AutoDark[idx][time];
								chart [idx].Dots [time] *= nframe / ++nframe;
								chart [idx].Dots [time] = values [y] / nframe;
							} else
								chart [idx].Dots.Add (time, values [y]);
						} catch {
						}
					});
					for (idx = 0; idx < correlator.NumLines; idx++) {
						if (line [idx].SelectedIndex > 2 || line [idx].SelectedIndex < 1)
							continue;
						double diff = (chart [idx].Dots.Values.Max () - chart [idx].Dots.Values.Min ()) * 0.2;
						diff = (diff == 0 ? 1 : diff);
						chart [idx].EndY = chart [idx].Dots.Values.Max () + diff;
						chart [idx].StartY = chart [idx].Dots.Values.Min () - diff;
					}
					double ysdiff = double.MaxValue;
					double yediff = 0;

					for (idx = 0; idx < correlator.NumLines; idx++) {
						if (line [idx].SelectedIndex > 2 || line [idx].SelectedIndex < 1)
							continue;
						ysdiff = Math.Min (ysdiff, chart [idx].StartY);
						yediff = Math.Max (yediff, chart [idx].EndY);
					}
					for (idx = 0; idx < correlator.NumLines; idx++) {
						if (line [idx].SelectedIndex > 2 || line [idx].SelectedIndex < 1)
							continue;
						chart [idx].StartY = ysdiff;
						chart [idx].EndY = yediff;
					}
					charts.StartY = ysdiff;
					charts.EndY = yediff;
				} catch {
				}
			} else if (e.Mode == OperatingMode.Counter) {
				int idx = e.Index;
				try {
					chart [idx].StartX = 0;
					var values = e.Counts;
					Parallel.For (0, (int)(TimeScale / correlator.PacketTime), delegate(int y) {
						try {
							double time = (double)y * correlator.PacketTime;
							if (chart [idx].Dots.ContainsKey (time)) {
								chart [idx].Dots [time] = values [values.Count - 1 - y] - Dark[idx];
							} else
								chart [idx].Dots.Add (time, values [values.Count - 1 - y]);
						} catch {
						}
					});
					for (idx = 0; idx < correlator.NumLines; idx++) {
						if (line [idx].SelectedIndex > 2 || line [idx].SelectedIndex < 1)
							continue;
						double diff = (chart [idx].Dots.Values.Max () - chart [idx].Dots.Values.Min ()) * 0.2;
						diff = (diff == 0 ? 1 : diff);
						chart [idx].EndY = chart [idx].Dots.Values.Max () + diff;
						chart [idx].StartY = chart [idx].Dots.Values.Min () - diff;
					}
					double ysdiff = double.MaxValue;
					double yediff = 0;

					for (idx = 0; idx < correlator.NumLines; idx++) {
						if (line [idx].SelectedIndex > 2 || line [idx].SelectedIndex < 1)
							continue;
						ysdiff = Math.Min (ysdiff, chart [idx].StartY);
						yediff = Math.Max (yediff, chart [idx].EndY);
					}
					for (idx = 0; idx < correlator.NumLines; idx++) {
						if (line [idx].SelectedIndex > 2 || line [idx].SelectedIndex < 1)
							continue;
						chart [idx].StartY = ysdiff;
						chart [idx].EndY = yediff;
					}
					charts.StartY = ysdiff;
					charts.EndY = yediff;
				} catch (Exception ex) {
					Console.WriteLine (ex.Message + Environment.NewLine + ex.StackTrace);
				}
			}
			this.Invoke ((MethodInvoker)delegate {
				charts.Invalidate (true);
			});
			correlator.SweepUpdate += Correlator_SweepUpdate;
		}
	}
}
