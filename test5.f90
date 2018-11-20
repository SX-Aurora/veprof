module a_module

contains

subroutine iamthe1percent
end subroutine iamthe1percent

end module a_module

subroutine iamthe99percent
end subroutine iamthe99percent

program main

use a_module

integer :: i,n

do i=1,10000000

  do n=1,99
     call iamthe99percent()
  end do
  call iamthe1percent()

end do

end program main
