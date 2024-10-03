

$fn = 100;

pcb = "ESP32AY.stl";

if (false)
{
  translate([0, 0, 0.6]) 
  {
    rotate([0,0,0]) color("lightgray") import(pcb);
  }
}


difference() {
    
    #import("ESP32AYTop.stl");

    translate([36, -115, -1.45])
        rotate([0, 90, 90])
            cube([4, 10, 30], true);
}

