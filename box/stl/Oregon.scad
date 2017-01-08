include <arduino.scad>

//Arduino boards
//You can create a boxed out version of a variety of boards by calling the arduino() module
//The default board for all functions is the Uno

content = "Oregon Scientific";
font1 = "Liberation Sans";
unoDimensions = boardDimensions( UNO );

//Board mockups
//translate([0, 0, -67]) {
//    arduino();
//}


translate([0, 0, -75]) {
	
    difference () {
        enclosure(UNO,3, 3, 20);
     translate([20,71, 20])   cube([16, 4, 13]); // holes in
    }
}

//translate([0, 0, -35]) {
translate([0, 0, 15]) {
	
         difference () {
             enclosureLid(UNO, 3, 3, 3, false);
          translate([8,5,2]) {  
               rotate ([0,0,50]) {
                    linear_extrude(2) text(content,font=font1,size=7);
                }
            }
        }
      //   translate([20,71, -19.75])   cube([16, 4, 13]); // holes in
      //   translate([11,67, -14])   cube([33, 4, 13]); // holes in
        difference () {
            translate([9.25,17, -9])   cube([37, 54, 8]); 
            translate([12.25,20, -10])   cube([30.5, 52, 8]); 
            
            translate([11.25,20, -6.6])   cube([1, 52, 1.6]); 
            translate([42.75,20, -6.6])   cube([1, 52, 1.6]); 
  
            
        }


}
