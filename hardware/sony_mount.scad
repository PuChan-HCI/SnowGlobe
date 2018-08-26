opteka_r1 = 24.75/2;
opteka_r2 = 41.35/2;

module rounded(size, r) {
    union() {
        translate([r, 0, 0]) cube([size[0]-2*r, size[1], size[2]]);
        translate([0, r, 0]) cube([size[0], size[1]-2*r, size[2]]);
        translate([r, r, 0]) cylinder(h=size[2], r=r);
        translate([size[0]-r, r, 0]) cylinder(h=size[2], r=r);
        translate([r, size[1]-r, 0]) cylinder(h=size[2], r=r);
        translate([size[0]-r, size[1]-r, 0]) cylinder(h=size[2], r=r);
    }
}

sony_size = [77.2, 150, 13.2];
sony_lip = 9.5;
sony_offset = [-sony_size[0] + 17.5, 0, -7.5];

gakken_lens_plate = [40, 17];
gakken_lens_center = [17, 12];
gakken_lens_r = 21.15/2;
gakken_lens_h = 10;

camera_w = 32;
camera_spacing = 28;
camera_keepout = 24;
camera_r = 1.0;
camera_d = 16;
camera_cable_keepout = [6, 13];

boss1_offset = [16, -15, 20];
boss2 = 60;
boss_depth = 6.5;

module gakken() {
    cylinder(r=gakken_lens_r, h=gakken_lens_h);
    translate([18, 2, gakken_lens_h/2]) cylinder(r=0.8, h=gakken_lens_h/2, $fn=20);
    translate([18, -9, gakken_lens_h/2]) cylinder(r=0.8, h=gakken_lens_h/2, $fn=20);
    translate([-15, 2, gakken_lens_h/2]) cylinder(r=0.8, h=gakken_lens_h/2, $fn=20);
    translate([-15, -9, gakken_lens_h/2]) cylinder(r=0.8, h=gakken_lens_h/2, $fn=20);
}

module sony() {
    translate(sony_offset) {
        union() {
            cube(sony_size);
            translate([0, sony_lip-1.5, -2]) cube([sony_size[0], 3, 2]);
        }
    }
}

margin = 0.1;
wall = 5;
wall2 = 2;
image_h = 10;

globe_r1 = 98/2;
globe_r2 = 105/2;
globe_h = 15;
globe_bolt = 3.8/2;

module lens_mount() difference() {
    translate([0, 0, -8]) union() {
        translate([0, 0, opteka_r2]) difference() {
            union() {
                // Mounting points for the screw bosses in the globe
                translate([boss1_offset[0], boss1_offset[1], boss1_offset[2]]) difference() {
                    translate([-wall2, -wall, -wall]) union() {
                        cube([wall2+boss_depth, boss2+wall*2, wall*2]);
                        rotate([-45, 0, 0])cube([wall2+boss_depth, 15, wall*2]);
                    }
                    rotate([0, 90, 0]) cylinder(r=5.2/2, h=20, $fn=20);
                    translate([wall, 0, 0]) rotate([0, -90, 0]) cylinder(r=1, h=20, $fn=20);
                    translate([0, boss2, 0]) rotate([0, 90, 0]) cylinder(r=5.2/2, h=20, $fn=20);
                    translate([wall, boss2, 0]) rotate([0, -90, 0]) cylinder(r=1, h=20, $fn=20);
                }
                
                // Support for the bar with the mounting points
                translate([boss1_offset[0]-wall2, -gakken_lens_h, -opteka_r2]) {
                    difference() {
                        cube([wall2+boss_depth, boss2, boss1_offset[2]-wall+opteka_r2]);
                        translate([-2.5, gakken_lens_h, opteka_r2-0.25]) rotate([0, -45, 0])  cube([wall2+boss_depth, boss2, boss1_offset[2]-wall+opteka_r2]);
                        translate([0, 15, 0]) cube([wall2+boss_depth, 6, boss1_offset[2]+10]);
                        translate([0, 50, 0])cube([wall2+boss_depth, 6, boss1_offset[2]+10]);
                    }
                }
                
                // Mounting plate for the lens
                translate([-gakken_lens_center[0], -gakken_lens_h, -gakken_lens_center[1]-wall]) {
                    cube([gakken_lens_plate[0], gakken_lens_h, gakken_lens_plate[1]+wall]);
                }
                
                translate([-gakken_lens_center[0]-camera_w, -gakken_lens_h-camera_d, -gakken_lens_center[1]-wall]) {
                    difference() {
                        union() {
                            cube([camera_w, gakken_lens_h+camera_d, wall*2]);
                            cube([camera_w, wall*2, wall+camera_w]);
                        }
                        translate([(camera_w-camera_keepout)/2,0,0]) cube([camera_keepout, wall/2, wall+camera_w]);
                        translate([0,0,(camera_w-camera_keepout)/2+wall]) cube([camera_w, wall/2, camera_keepout]);
                        translate([(camera_w-camera_keepout)/2,0,camera_w-camera_cable_keepout[0]+wall]) cube([camera_cable_keepout[1], wall*3, camera_cable_keepout[0]]);
                        for (i = [0, camera_spacing]) {
                            for (j = [0, camera_spacing]) {
                                translate([(camera_w-camera_spacing)/2+i,0,wall+camera_r*2+j]) rotate([-90,0,0]) cylinder(r=1.0, h=wall*2, $fn=20);
                            }
                        }
                    }
                }
            }
            
            // Cut-out for the lens barrel and screw holes
            minkowski() {
                rotate([90, 0, 0]) gakken();
                translate([-margin, -margin, -margin]) cube([margin*2, margin*2, margin]);
            }
        }
    }
    
    translate([-200, -200, -100]) cube([400, 400, 100]);
}

difference() {
    union() {
        lens_mount();
        intersection() {
            sony();
            translate(sony_offset) cube([sony_size[0], boss2-gakken_lens_h, sony_size[2]]);
        }
    }
    translate([0, 0, opteka_r2-8]) sony();
    translate([-200, -200, -100]) cube([400, 400, 100]);
}
