/*   Simple Chart WinForms code
    Copyright (C) 2017  Ilia Platone <info_at_iliaplatone.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

using System;
using System.Drawing;
using System.Windows.Forms;
using System.Collections.ObjectModel;
using System.Linq;
using System.Collections.Generic;

namespace Crosscorrelator
{
	public class Graph : PictureBox
	{
		double independent = 1.0;
		double dependent = 1.0;
		double startx = -1.0;
		double starty = -1.0;
		int smooth = 3;
		Dictionary<double, double> dots = new Dictionary<double, double> ();

		public string LabelX = "";
		public string LabelY = "";

		public Dictionary<double, double> Dots {
			get { return dots; }
		}

		public double EndX {
			get {
				return independent;
			}
			set {
				independent = value;
			}
		}

		public double EndY {
			get {
				return dependent;
			}
			set {
				dependent = value;
			}
		}

		public double StartX {
			get {
				return startx;
			}
			set {
				startx = value;
			}
		}

		public double StartY {
			get {
				return starty;
			}
			set {
				starty = value;
			}
		}

		public int Smooth {
			get {
				return smooth;
			}
			set {
				smooth = value;
			}
		}

		public Graph()
			: base()
		{
			InitializeComponent();
		}

		void InitializeComponent()
		{
			this.Controls.Clear ();
			try {
				this.ClientSize = new Size (475, 265);
			} catch (Exception ex) {
				MessageBox.Show (ex.Message, this.GetType ().Name + " Plugin error");
			}
		}

		public void display()
		{
			Bitmap uv = new Bitmap (this.Width, this.Height);
			using (Graphics g = Graphics.FromImage (uv)) {
				try {
					g.FillRectangle (Brushes.White, new Rectangle (0, 0, uv.Width, uv.Height));
					if(EndX == StartX)
						EndX += 0.1;
					if(EndY == StartY)
						EndY += 0.1;
					if (EndX > StartX) {
						double b = Math.Abs (EndX - StartX);
						double sx = StartX;
						double exp = 0;
						if(b == 0)
							b = 1;
						if (b < 10) {
							for (exp = 0; b < 10; exp--) {
								b *= 10;
								sx *= 10;
							}
						}
						if (b > 100) {
							for (exp = 0; b > 100; exp++) {
								b /= 10;
								sx /= 10;
							}
						}
						int y = 0;
						for (double x = (int)Math.Min(sx, b + sx); y < uv.Width; x++) {
							y = (int)((x - sx) * uv.Width / b);
							g.DrawLine (Pens.Black, new Point (y, uv.Height - 1), new Point (y, uv.Height - 3));
							if (((int)x % 5) == 0) {
								g.DrawLine(Pens.LightGray, new Point ( y, 0), new Point( y, uv.Height - 1));
								g.DrawLine (Pens.Black, new Point (y, uv.Height - 1), new Point (y, uv.Height - 4));
							}
							if (((int)x % ((int)(b / 10) * 5)) == 0) {
								string val = (x * Math.Pow (10, exp)).ToString ();
								g.DrawString (val, Font, Brushes.Black, new Rectangle (y - val.Length * 5, uv.Height - 20, val.Length * 10, 30));
							}
						}
						b = Math.Abs (EndY - StartY);
						sx = StartY;
						exp = 0;
						if (b < 10) {
							for (exp = 0; b < 10; exp--) {
								b *= 10;
								sx *= 10;
							}
						}
						if (b > 100) {
							for (exp = 0; b > 100; exp++) {
								b /= 10;
								sx /= 10;
							}
						}
						y = 0;
						for (double x = (int)Math.Min(sx, b + sx); y >= 0; x++) {
							double v = (x - sx);
							y = (int)(int)(uv.Height - 1 - v * uv.Height / b);
							g.DrawLine (Pens.Black, new Point (0, y), new Point (2, y));
							if (((int)x % 5) == 0) {
								g.DrawLine(Pens.LightGray, new Point (0, y), new Point( uv.Width - 1, y));
								g.DrawLine (Pens.Black, new Point (0, y), new Point (3, y));
							}
							if (((int)x % ((int)(b / 10) * 5)) == 0) {
								string val = (x * Math.Pow (10, exp)).ToString ();
								g.DrawString (val, Font, Brushes.Black, new Rectangle (10, y - 5, val.Length * 10, 30));
							}
						}
						if (Dots.Keys.Count > 2) {
							Collection<Point> curve = new Collection<Point> ();
							Collection<Point> average = new Collection<Point> ();
							double mean = 0;
							double minx = StartX;
							double maxx = EndX - minx;
							double miny = StartY;
							double maxy = EndY - miny;
							var sorted = Dots.Keys.ToList ();
							sorted.Sort ();
							for (int key = 0; key < sorted.Count; key++) {
								
								double u = sorted [key];
								u *= uv.Width / (maxx != 0 ? maxx : 1);
								u -= minx * uv.Width / maxx;

								double v = Dots[sorted[key]];
								v *= uv.Height / (maxy != 0 ? maxy : 1);
								v -= miny * uv.Height / maxy;
								v = uv.Height - v - 1;

								curve.Add (new Point ((int)u, (int)v));
							}
							mean = curve[0].Y;
							int i = 0;
							double diff = 0;
							for(int x = 0; x < uv.Width-Smooth; x++)
							{
								for(int z = 0; z < Smooth; z++)
									diff += (double)curve[i+z].Y/((double)uv.Width/(double)curve.Count);
								if(curve[i].X<x && i < curve.Count-Smooth)
									i++;
								diff /= Smooth;
								average.Add(new Point((int)x, (int)diff));
							}
							foreach(Point dot in curve)
							{
								g.DrawEllipse(Pens.Blue, new Rectangle(dot.X - 2, dot.Y - 2, 4, 4));
							}
							g.DrawLines (Pens.LightBlue, curve.ToArray ());
							g.DrawLines (Pens.Red, average.ToArray ());
						}
						g.DrawString (LabelX, Font, Brushes.Black, new Rectangle (uv.Width - 20 - LabelX.Length * 8, uv.Height - 35, LabelX.Length * 8 + 1, 30));
						g.DrawString (LabelY, Font, Brushes.Black, new Rectangle (20, 5, LabelY.Length * 8 + 1, 30));
					}
				} catch (Exception ex) {
					Console.WriteLine (ex.Message + Environment.NewLine + ex.StackTrace);
				}
			}
			this.Invoke ((MethodInvoker)delegate {
				this.Image = uv;
				Invalidate (true);
			});
		}
	}
}