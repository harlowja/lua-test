blah = 2.0

-- This function must exist!
function build(vehicle, component)
    local w = {x=0, y=0, vehicle=vehicle, z=false, blah=blah, component=component}
    return w
end
